import { def } from '../utils';
import { objectUrls } from '../polyfills/url';
import { decoder } from '../polyfills/text-decoder';
import { Request, type RequestInit } from './request';
import { Response } from './response';
import { Headers } from './headers';
import type { SwitchClass } from '../switch';

declare const Switch: SwitchClass;

function fdToStream(fd: number) {
	return new ReadableStream<Uint8Array>({
		async pull(controller) {
			const buffer = new ArrayBuffer(65536);
			const bytesRead = await Switch.read(fd, buffer);
			if (bytesRead === 0) {
				controller.close();
			} else {
				controller.enqueue(new Uint8Array(buffer.slice(0, bytesRead)));
			}
		},
	});
}

function indexOfEol(arr: ArrayLike<number>, offset: number): number {
	for (let i = offset; i < arr.length - 1; i++) {
		if (arr[i] === 13 && arr[i + 1] === 10) {
			return i;
		}
	}
	return -1;
}

function concat(a: Uint8Array, b: Uint8Array) {
	const c = new Uint8Array(a.length + b.length);
	c.set(a, 0);
	c.set(b, a.length);
	return c;
}

async function* headersIterator(
	reader: ReadableStreamDefaultReader<Uint8Array>
): AsyncGenerator<{ line: string } | { line: null; leftover: Uint8Array }> {
	let leftover: Uint8Array | null = null;
	while (true) {
		const next = await reader.read();
		if (next.done) throw new Error('Stream closed before end of headers');
		const chunk: Uint8Array = leftover
			? concat(leftover, next.value)
			: next.value;
		let pos = 0;
		while (true) {
			const eol = indexOfEol(chunk, pos);
			if (eol === -1) {
				leftover = chunk.slice(pos);
				break;
			}
			const line = decoder.decode(chunk.slice(pos, eol));
			pos = eol + 2;
			if (line) {
				yield { line };
			} else {
				// end of headers
				leftover = chunk.slice(pos);
				yield { line: null, leftover };
				reader.releaseLock();
				return;
			}
		}
	}
}

function createChunkedParseStream() {
	let dataSize = -1;
	let buffer: Uint8Array | null = null;

	return new TransformStream<Uint8Array, Uint8Array>({
		transform(chunk, controller) {
			buffer = buffer ? concat(buffer, chunk) : chunk;
			//console.log({ c: this.buffer })

			if (dataSize !== -1) {
				if (buffer.length >= dataSize + 2) {
					const chunkData = buffer.slice(0, dataSize);
					buffer = buffer.slice(dataSize + 2);
					controller.enqueue(chunkData);
					dataSize = -1;
				} else {
					//console.log('waiting for more');
					return; // not enough data, wait for more
				}
			}

			let pos;
			while ((pos = indexOfEol(buffer, 0)) >= 0) {
				// while we can find a chunk boundary
				//console.log({ pos })
				if (pos === 0) {
					// skip empty chunks
					buffer = buffer.slice(2);
					continue;
				}

				const size = parseInt(decoder.decode(buffer.slice(0, pos)), 16);
				//console.log({ size });
				if (isNaN(size)) {
					throw new Error('Invalid chunk size');
				}
				buffer = buffer.slice(pos + 2);

				if (buffer.length >= size + 2) {
					// we got a whole chunk
					const chunkData = buffer.slice(0, size);
					//console.log({ chunkData });
					buffer = buffer.slice(size + 2);
					controller.enqueue(chunkData);
				} else {
					dataSize = size;
					//console.log(`waiting for more (needs: ${size}, has: ${buffer.length})`);
					break; // not enough data, wait for more
				}
			}
		},

		flush(controller) {
			// in case there's unprocessed data left in the buffer when the stream is closed
			if (buffer && buffer.length > 0) {
				controller.enqueue(buffer);
				buffer = null;
			}
		},
	});
}

async function fetchHttp(req: Request, url: URL) {
	const { hostname } = url;
	const port = +url.port || 80;
	const fd = await Switch.connect({ hostname, port });

	req.headers.set('connection', 'close');
	if (!req.headers.has('host')) {
		req.headers.set('host', url.host);
	}
	if (!req.headers.has('user-agent')) {
		req.headers.set('user-agent', `nx.js/${Switch.version.nxjs}`);
	}
	if (!req.headers.has('accept')) {
		req.headers.set('accept', '*/*');
	}

	let header = `${req.method} ${url.pathname}${url.search} HTTP/1.1\r\n`;
	for (const [name, value] of req.headers) {
		header += `${name}: ${value}\r\n`;
	}
	header += '\r\n';
	await Switch.write(fd, header);

	const resHeaders = new Headers();
	const s = fdToStream(fd);
	const r = s.getReader();

	const hi = headersIterator(r);

	// Parse response status
	const firstLine = await hi.next();
	if (firstLine.done || !firstLine.value.line) {
		throw new Error('Failed to read response header');
	}
	const [_, status, statusText] = firstLine.value.line.split(' ');

	// Parse response headers
	let leftover: Uint8Array | undefined;
	for await (const v of hi) {
		if (typeof v.line === 'string') {
			const col = v.line.indexOf(':');
			const name = v.line.slice(0, col);
			const value = v.line.slice(col + 1).trim();
			resHeaders.set(name, value);
		} else {
			leftover = v.leftover;
		}
	}

	const resStream =
		resHeaders.get('transfer-encoding') === 'chunked'
			? createChunkedParseStream()
			: new TransformStream();
	if (leftover) {
		const w = resStream.writable.getWriter();
		w.write(leftover);
		w.releaseLock();
		leftover = undefined;
	}
	s.pipeThrough(resStream);

	return new Response(resStream.readable, {
		status: +status,
		statusText,
		headers: resHeaders,
	});
}

async function fetchBlob(req: Request, url: URL) {
	if (req.method !== 'GET') {
		throw new Error(
			`GET method must be used when fetching "${url.protocol}" protocol (got "${req.method}")`
		);
	}
	const data = objectUrls.get(req.url);
	if (!data) {
		throw new Error(`Object URL "${req.url}" does not exist`);
	}
	return new Response(data, {
		headers: {
			'content-length': String(data.size),
		},
	});
}

async function fetchFile(req: Request, url: URL) {
	if (req.method !== 'GET') {
		throw new Error(
			`GET method must be used when fetching "${url.protocol}" protocol (got "${req.method}")`
		);
	}
	const path = url.protocol === 'file:' ? `sdmc:${url.pathname}` : url.href;
	const data = await Switch.readFile(path);
	return new Response(data, {
		headers: {
			'content-length': String(data.byteLength),
		},
	});
}

const fetchers = new Map<string, (req: Request, url: URL) => Promise<Response>>(
	[
		['http:', fetchHttp],
		['blob:', fetchBlob],
		['file:', fetchFile],
		['sdmc:', fetchFile],
		['romfs:', fetchFile],
	]
);

/**
 * The global `fetch()` method starts the process of fetching a resource from the network, returning a promise which is fulfilled once the response is available.
 *
 * ### Supported Protocols
 *
 *  - `http:` - Fetch data from the network using the HTTP protocol
 *  - `blob:` - Fetch data from a URL constructed by {@link URL.createObjectURL | `URL.createObjectURL()`}
 *  - `sdmc:` - Fetch data from a local file on the SD card
 *  - `romfs:` - Fetch data from the RomFS partition of the nx.js application
 *  - `file:` - Same as `sdmc:`
 *
 * @example
 *
 * ```typescript
 * fetch('http://jsonip.com')
 *   .then(res => res.json())
 *   .then(data => {
 *     console.log(`Current IP address: ${data.ip}`);
 *   });
 * ```
 *
 * @see https://developer.mozilla.org/en-US/docs/Web/API/fetch
 */
export function fetch(input: string | URL | Request, init?: RequestInit) {
	const req = new Request(input, init);
	const url = new URL(req.url);
	const fetcher = fetchers.get(url.protocol);
	if (!fetcher) {
		throw new Error(`scheme '${url.protocol.slice(0, -1)}' not supported`);
	}
	return fetcher(req, url);
}
def('fetch', fetch);
