import { decoder } from '../polyfills/text-decoder';
import { encoder } from '../polyfills/text-encoder';
import { Headers, type HeadersInit } from './headers';
import { crypto } from '../crypto';
import { asyncIteratorToStream, bufferSourceToArrayBuffer } from '../utils';
import { Blob } from '../polyfills/blob';
import { File } from '../polyfills/file';
import { FormData } from '../polyfills/form-data';
import { URLSearchParams } from '../polyfills/url';
import type { BufferSource } from '../types';

function indexOfSequence<T>(
	haystack: ArrayLike<T>,
	needle: ArrayLike<T>,
	offset = 0,
) {
	if (needle.length === 0) return -1;

	for (let i = offset; i <= haystack.length - needle.length; i++) {
		let found = true;

		for (let j = 0; j < needle.length; j++) {
			if (haystack[i + j] !== needle[j]) {
				found = false;
				break;
			}
		}

		if (found) return i;
	}

	return -1;
}

async function* stringIterator(s: string) {
	yield encoder.encode(s);
}

async function* arrayBufferIterator(b: ArrayBuffer) {
	yield new Uint8Array(b);
}

async function* formDataIterator(f: FormData, boundary: string) {
	for (const [name, value] of f) {
		let header = `${boundary}\r\nContent-Disposition: form-data; name="${name}"`;
		if (value instanceof File && value.name) {
			header += `; filename="${value.name}"\r\n`;
		} else {
			header += '\r\n';
		}
		if (value instanceof Blob && value.type) {
			header += `Content-Type: ${value.type}\r\n`;
		}
		header += '\r\n';
		yield encoder.encode(header);
		yield typeof value === 'string'
			? encoder.encode(value)
			: new Uint8Array(await value.arrayBuffer());
		yield encoder.encode('\r\n');
	}
	yield encoder.encode(`${boundary}--`);
}

export type BodyInit =
	| ReadableStream<any>
	| Blob
	| BufferSource
	| FormData
	| URLSearchParams
	| string;

export abstract class Body implements globalThis.Body {
	body: ReadableStream<Uint8Array> | null;
	bodyUsed: boolean;
	headers: Headers;

	constructor(init?: Body | BodyInit | null, headers?: HeadersInit) {
		this.body = null;
		this.bodyUsed = false;
		this.headers = new Headers(headers);
		let contentType: string | undefined;

		if (init && typeof init === 'object' && 'body' in init) {
			if (init.bodyUsed) {
				throw new Error("Input request's body is unusable");
			}
			init.bodyUsed = true;
			init = init.body;
		}

		if (init) {
			if (typeof init === 'string') {
				this.body = asyncIteratorToStream(stringIterator(init));
				contentType = 'text/plain;charset=UTF-8';
			} else if (init instanceof Blob) {
				this.body = init.stream();
				contentType = init.type;
			} else if (init instanceof URLSearchParams) {
				this.body = asyncIteratorToStream(stringIterator(String(init)));
				contentType = 'application/x-www-form-urlencoded;charset=UTF-8';
			} else if (init instanceof ReadableStream) {
				if (init.locked) {
					throw new TypeError('ReadableStream is locked or disturbed');
				}
				this.body = init;
			} else if (init instanceof FormData) {
				const boundary = `------${crypto.randomUUID().replace(/-/g, '')}`;
				this.body = asyncIteratorToStream(formDataIterator(init, boundary));
				contentType = `multipart/form-data; boundary=${boundary}`;
			} else {
				this.body = asyncIteratorToStream(
					arrayBufferIterator(bufferSourceToArrayBuffer(init)),
				);
			}
		}

		if (contentType && !this.headers.has('content-type')) {
			this.headers.set('content-type', contentType);
		}
	}

	/**
	 * Returns a promise that resolves with an ArrayBuffer representation of the body.
	 * If the body is null, it returns an empty ArrayBuffer.
	 */
	async arrayBuffer(): Promise<ArrayBuffer> {
		if (!this.body) return new ArrayBuffer(0);
		let bytes = 0;
		const chunks: Uint8Array[] = [];
		const reader = this.body.getReader();
		while (true) {
			const next = await reader.read();
			if (next.done) break;
			chunks.push(next.value);
			bytes += next.value.length;
		}
		this.bodyUsed = true;
		const arr = new Uint8Array(bytes);
		let offset = 0;
		for (const chunk of chunks) {
			arr.set(chunk, offset);
			offset += chunk.length;
		}
		return arr.buffer;
	}

	/**
	 * Returns a promise that resolves with a {@link Blob} representation of the body.
	 * The Blob's type will be the value of the 'content-type' header.
	 */
	async blob(): Promise<Blob> {
		const buf = await this.arrayBuffer();
		const type = this.headers.get('content-type') ?? undefined;
		return new Blob([buf], { type });
	}

	/**
	 * Returns a promise that resolves with a {@link FormData} representation of the body.
	 * If the body cannot be decoded as form data, it throws a `TypeError`.
	 */
	async formData(): Promise<FormData> {
		const contentType = this.headers.get('content-type');
		if (!contentType) {
			throw new TypeError(
				'Could not parse content as FormData (missing "content-type" header)',
			);
		}
		const form = new FormData();
		if (contentType === 'application/x-www-form-urlencoded') {
			const text = await this.text();
			for (const entry of text.split('&')) {
				const eq = entry.indexOf('=');
				const k = decodeURIComponent(entry.slice(0, eq));
				const v = decodeURIComponent(entry.slice(eq + 1));
				form.append(k, v);
			}
		} else if (contentType.startsWith('multipart/form-data;')) {
			const boundary = contentType
				.split(/;\s?/)
				.find((p) => p.startsWith('boundary='))
				?.slice(9);
			if (!boundary) {
				throw new TypeError(
					'Could not parse content as FormData (missing "boundary" in "content-type" header)',
				);
			}
			const boundaryBytes = encoder.encode(boundary);
			const data = new Uint8Array(await this.arrayBuffer());
			let pos = 0;
			let start;
			const offsets: number[] = [];
			while ((start = indexOfSequence(data, boundaryBytes, pos)) !== -1) {
				offsets.push(start);
				pos += start + boundaryBytes.length;
			}
			for (let i = 0; i < offsets.length; i++) {
				const part = data.subarray(
					offsets[i] + boundaryBytes.length + 2,
					offsets[i + 1],
				);
				if (part.length === 0) break;
				pos = 0;
				let eol;
				let name: string | undefined;
				let filename: string | undefined;
				let type: string | undefined;
				while ((eol = indexOfSequence(part, [13, 10], pos)) !== -1) {
					const header = part.subarray(pos, eol);
					pos = eol + 2;
					if (!header.length) {
						break;
					}
					const h = decoder.decode(header);
					const colon = h.indexOf(':');
					const key = h.slice(0, colon).toLowerCase();
					const value = h.slice(colon + 1).trimStart();
					if (key === 'content-disposition') {
						const disposition = value.split(/;\s*/);
						name = disposition
							.find((p) => p.startsWith('name='))
							?.slice(5)
							.replace(/^"|"$/g, '');
						filename = disposition
							.find((p) => p.startsWith('filename='))
							?.slice(9)
							.replace(/^"|"$/g, '');
					} else if (key === 'content-type') {
						type = value;
					}
				}
				if (!name) {
					throw new TypeError(
						'No "name" provided in `Content-Disposition` header',
					);
				}
				const valueBytes = part.subarray(pos, part.length - 2);
				const value = filename
					? new File([valueBytes], filename, { type })
					: decoder.decode(valueBytes);
				form.append(name, value);
			}
		} else {
			throw new TypeError(
				`Could not parse content as FormData ("content-type" header must be "application/x-www-form-urlencoded" or "multipart/form-data", got "${contentType}")`,
			);
		}
		return form;
	}

	/**
	 * Returns a promise that resolves with a JSON representation of the body.
	 * If the body cannot be parsed as JSON, it throws a `SyntaxError`.
	 */
	async json(): Promise<any> {
		const text = await this.text();
		return JSON.parse(text);
	}

	/**
	 * Returns a promise that resolves with a text representation of the body.
	 */
	async text(): Promise<string> {
		const buf = await this.arrayBuffer();
		return decoder.decode(buf);
	}
}
