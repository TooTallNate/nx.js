import { bufferSourceToArrayBuffer } from '../utils';

function stringToStream(text: string) {
	const encoder = new TextEncoder();
	const uint8array = encoder.encode(text);
	return new ReadableStream<Uint8Array>({
		start(controller) {
			controller.enqueue(uint8array);
			controller.close();
		},
	});
}

function blobToStream(blob: Blob) {
	return new ReadableStream<Uint8Array>({
		async pull(controller) {
			const data = await blob.arrayBuffer();
			controller.enqueue(new Uint8Array(data));
			controller.close();
		},
	});
}

function arrayBufferToStream(buf: ArrayBuffer) {
	return new ReadableStream<Uint8Array>({
		start(controller) {
			controller.enqueue(new Uint8Array(buf));
			controller.close();
		},
	});
}

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
				this.body = stringToStream(init);
				contentType = 'text/plain;charset=UTF-8';
			} else if (init instanceof Blob) {
				this.body = blobToStream(init);
				contentType = init.type;
			} else if (init instanceof URLSearchParams) {
				this.body = stringToStream(String(init));
				contentType = 'application/x-www-form-urlencoded;charset=UTF-8';
			} else if (init instanceof ReadableStream) {
				if (init.locked) {
					throw new TypeError(
						'ReadableStream is locked or disturbed'
					);
				}
				this.body = init;
			} else {
				// TODO: handle `FormData`
				this.body = arrayBufferToStream(
					bufferSourceToArrayBuffer(init as BufferSource)
				);
			}
		}

		if (contentType && !this.headers.has('content-type')) {
			this.headers.set('content-type', contentType);
		}
	}

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

	async blob(): Promise<Blob> {
		const buf = await this.arrayBuffer();
		const type = this.headers.get('content-type') ?? undefined;
		return new Blob([buf], { type });
	}

	async formData(): Promise<FormData> {
		const body = await this.text();
		const form = new FormData();
		for (const bytes of body.trim().split('&')) {
			if (!bytes) continue;
			const split = bytes.split('=');
			const name = split.shift()?.replace(/\+/g, ' ');
			if (!name) continue;
			const value = split.join('=').replace(/\+/g, ' ');
			form.append(decodeURIComponent(name), decodeURIComponent(value));
		}
		return form;
	}

	async json(): Promise<any> {
		const text = await this.text();
		return JSON.parse(text);
	}

	async text(): Promise<string> {
		const buf = await this.arrayBuffer();
		return new TextDecoder().decode(buf);
	}
}
