import { dataUriToBuffer } from 'data-uri-to-buffer';
import { def } from '../utils';
import { readFile } from '../fs';
import { objectUrls } from '../polyfills/url';
import { URL } from '../polyfills/url';
import { decoder } from '../polyfills/text-decoder';
import { encoder } from '../polyfills/text-encoder';
import { Request, type RequestInit } from './request';
import { Response } from './response';
import { Headers } from './headers';
import { navigator } from '../navigator';
import { Socket, connect } from '../tcp';
import { INTERNAL_SYMBOL } from '../internal';

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
	reader: ReadableStreamDefaultReader<Uint8Array>,
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

			if (dataSize !== -1) {
				if (buffer.length >= dataSize + 2) {
					const chunkData = buffer.slice(0, dataSize);
					buffer = buffer.slice(dataSize + 2);
					controller.enqueue(chunkData);
					dataSize = -1;
				} else {
					return; // not enough data, wait for more
				}
			}

			let pos;
			while ((pos = indexOfEol(buffer, 0)) >= 0) {
				// while we can find a chunk boundary
				if (pos === 0) {
					// skip empty chunks
					buffer = buffer.slice(2);
					continue;
				}

				const size = parseInt(decoder.decode(buffer.slice(0, pos)), 16);
				if (isNaN(size)) {
					throw new Error('Invalid chunk size');
				}
				buffer = buffer.slice(pos + 2);

				if (buffer.length >= size + 2) {
					// we got a whole chunk
					const chunkData = buffer.slice(0, size);
					buffer = buffer.slice(size + 2);
					controller.enqueue(chunkData);
				} else {
					dataSize = size;
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

async function fetchHttp(req: Request, url: URL): Promise<Response> {
	const isHttps = url.protocol === 'https:';
	const { hostname } = url;
	const port = +url.port || (isHttps ? 443 : 80);
	const socket = new Socket(
		// @ts-expect-error Internal constructor
		INTERNAL_SYMBOL,
		{ hostname, port },
		{ secureTransport: isHttps ? 'on' : 'off', connect },
	);

	req.headers.set('connection', 'close');
	if (!req.headers.has('host')) {
		req.headers.set('host', url.host);
	}
	if (!req.headers.has('user-agent')) {
		req.headers.set('user-agent', navigator.userAgent);
	}
	if (!req.headers.has('accept')) {
		req.headers.set('accept', '*/*');
	}

	let header = `${req.method} ${url.pathname}${url.search} HTTP/1.1\r\n`;
	for (const [name, value] of req.headers) {
		header += `${name}: ${value}\r\n`;
	}
	header += '\r\n';
	const w = socket.writable.getWriter();
	await w.write(encoder.encode(header));

	const resHeaders = new Headers();
	const r = socket.readable.getReader();

	const hi = headersIterator(r);

	// Parse response status
	const firstLine = await hi.next();
	if (firstLine.done || !firstLine.value.line) {
		throw new Error('Failed to read response header');
	}
	const [_, statusStr, statusText] = firstLine.value.line.split(' ');
	const status = +statusStr;

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

	// Redirect
	if (((status / 100) | 0) === 3) {
		if (req.redirect === 'follow') {
			socket.readable.cancel();
			w.close();
			const loc = resHeaders.get('location');
			if (!loc) {
				throw new Error(
					`No "Location" header in ${status} redirect from "${url}"`,
				);
			}
			const redirectUrl = new URL(loc, req.url);
			let method: RequestInit['method'] = 'GET';
			let body: RequestInit['body'] = null;
			if (status === 307 || status === 308) {
				method = req.method;
				body = req.body;
			}
			const redirect = new Request(redirectUrl, { method, body });
			const res = await fetchHttp(redirect, redirectUrl);
			res.redirected = true;
			return res;
		}

		if (req.redirect === 'error') {
		}

		// For "manual", just continue with the regular logic
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
	socket.readable.pipeThrough(resStream);

	const res = new Response(resStream.readable, {
		status,
		statusText,
		headers: resHeaders,
	});
	res.url = url.href;
	return res;
}

async function fetchBlob(req: Request, url: URL) {
	if (req.method !== 'GET') {
		throw new Error(
			`GET method must be used when fetching "${url.protocol}" protocol (got "${req.method}")`,
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

async function fetchData(req: Request, url: URL) {
	if (req.method !== 'GET') {
		throw new Error(
			`GET method must be used when fetching "${url.protocol}" protocol (got "${req.method}")`,
		);
	}
	const parsed = dataUriToBuffer(url);
	return new Response(parsed.buffer, {
		headers: {
			'content-length': String(parsed.buffer.byteLength),
			'content-type': parsed.typeFull,
		},
	});
}

async function fetchFile(req: Request, url: URL) {
	if (req.method !== 'GET') {
		throw new Error(
			`GET method must be used when fetching "${url.protocol}" protocol (got "${req.method}")`,
		);
	}
	const path = url.protocol === 'file:' ? `sdmc:${url.pathname}` : url.href;
	// TODO: Use streaming FS interface
	const data = await readFile(path);
	const headers = new Headers();
	let status = 200;
	if (data) {
		headers.set('content-length', String(data.byteLength));
	} else {
		status = 404;
	}
	return new Response(data, { status, headers });
}

const fetchers = new Map<string, (req: Request, url: URL) => Promise<Response>>(
	[
		['http:', fetchHttp],
		['https:', fetchHttp],
		['blob:', fetchBlob],
		['data:', fetchData],
		['file:', fetchFile],
		['sdmc:', fetchFile],
		['romfs:', fetchFile],
	],
);

/**
 * The global `fetch()` method starts the process of fetching a resource from the network, returning a promise which is fulfilled once the response is available.
 *
 * ### Supported Protocols
 *
 * | Protocol | Description                                                                 |
 * |----------|-----------------------------------------------------------------------------|
 * | `http:`  | Fetch data from the network using the HTTP protocol                         |
 * | `https:` | Fetch data from the network using the HTTPS protocol                        |
 * | `blob:`  | Fetch data from a URL constructed by {@link URL.createObjectURL | `URL.createObjectURL()`}                |
 * | `data:`  | Fetch data from a [Data URI](https://developer.mozilla.org/docs/Web/HTTP/Basics_of_HTTP/Data_URLs) (possibly base64-encoded)                        |
 * | `sdmc:`  | Fetch data from a local file on the SD card                                 |
 * | `romfs:` | Fetch data from the RomFS partition of the nx.js application                |
 * | `file:`  | Same as `sdmc:`                                                             |
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
 * @see https://developer.mozilla.org/docs/Web/API/fetch
 */
export async function fetch(
	input: string | URL | Request,
	init?: RequestInit,
): Promise<Response> {
	const req = new Request(input, init);
	const url = new URL(req.url);
	const fetcher = fetchers.get(url.protocol);
	if (!fetcher) {
		throw new Error(`scheme '${url.protocol.slice(0, -1)}' not supported`);
	}
	const res = await fetcher(req, url);
	if (!res.url) res.url = url.href;
	return res;
}
def(fetch);
