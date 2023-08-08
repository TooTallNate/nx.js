import { def } from '../utils';
import { encoder } from './text-encoder';

/*! fetch-blob. MIT License. Jimmy Wärting <https://jimmy.warting.se/opensource> */

// 64 KiB (same size chrome slice theirs blob into Uint8array's)
const POOL_SIZE = 65536;

async function* toIterator(
	parts: (Blob | Uint8Array)[],
	clone: boolean
): AsyncIterableIterator<Uint8Array> {
	for (const part of parts) {
		if (ArrayBuffer.isView(part)) {
			if (clone) {
				let position = part.byteOffset;
				const end = part.byteOffset + part.byteLength;
				while (position !== end) {
					const size = Math.min(end - position, POOL_SIZE);
					const chunk = part.buffer.slice(position, position + size);
					position += chunk.byteLength;
					yield new Uint8Array(chunk);
				}
			} else {
				yield part;
			}
		} else {
			// @ts-ignore TS ReadableStream does not have `Symbol.asyncIterator`
			yield* part.stream();
		}
	}
}

export class Blob implements globalThis.Blob {
	#parts: (Blob | Uint8Array)[] = [];
	#type = '';
	#size = 0;
	#endings = 'transparent';

	constructor(blobParts: BlobPart[] = [], options: BlobPropertyBag = {}) {
		if (typeof blobParts !== 'object' || blobParts === null) {
			throw new TypeError(
				"Failed to construct 'Blob': The provided value cannot be converted to a sequence."
			);
		}

		if (typeof blobParts[Symbol.iterator] !== 'function') {
			throw new TypeError(
				"Failed to construct 'Blob': The object must have a callable @@iterator property."
			);
		}

		if (typeof options !== 'object' && typeof options !== 'function') {
			throw new TypeError(
				"Failed to construct 'Blob': parameter 2 cannot convert to dictionary."
			);
		}

		for (const element of blobParts) {
			let part;
			if (ArrayBuffer.isView(element)) {
				part = new Uint8Array(
					element.buffer.slice(
						element.byteOffset,
						element.byteOffset + element.byteLength
					)
				);
			} else if (element instanceof ArrayBuffer) {
				part = new Uint8Array(element.slice(0));
			} else if (element instanceof Blob) {
				part = element;
			} else {
				part = encoder.encode(`${element}`);
			}

			const size = ArrayBuffer.isView(part) ? part.byteLength : part.size;
			// Avoid pushing empty parts into the array to better GC them
			if (size) {
				this.#size += size;
				this.#parts.push(part);
			}
		}

		this.#endings = `${
			options.endings === undefined ? 'transparent' : options.endings
		}`;
		const type = options.type === undefined ? '' : String(options.type);
		this.#type = /^[\x20-\x7E]*$/.test(type) ? type : '';
	}

	get size() {
		return this.#size;
	}

	get type() {
		return this.#type;
	}

	async text(): Promise<string> {
		// More optimized than using this.arrayBuffer()
		// that requires twice as much ram
		const decoder = new TextDecoder();
		let str = '';
		for await (const part of toIterator(this.#parts, false)) {
			str += decoder.decode(part, { stream: true });
		}
		// Remaining
		str += decoder.decode();
		return str;
	}

	async arrayBuffer(): Promise<ArrayBuffer> {
		const data = new Uint8Array(this.size);
		let offset = 0;
		for await (const chunk of toIterator(this.#parts, false)) {
			data.set(chunk, offset);
			offset += chunk.length;
		}

		return data.buffer;
	}

	stream() {
		const it = toIterator(this.#parts, true);

		return new ReadableStream({
			type: 'bytes',

			async pull(ctrl) {
				const chunk = await it.next();
				chunk.done ? ctrl.close() : ctrl.enqueue(chunk.value);
			},

			async cancel() {
				await it.return!();
			},
		});
	}

	slice(start: number = 0, end: number = this.size, type: string = '') {
		const { size } = this;

		let relativeStart =
			start < 0 ? Math.max(size + start, 0) : Math.min(start, size);
		let relativeEnd =
			end < 0 ? Math.max(size + end, 0) : Math.min(end, size);

		const span = Math.max(relativeEnd - relativeStart, 0);
		const parts = this.#parts;
		const blobParts = [];
		let added = 0;

		for (const part of parts) {
			// don't add the overflow to new blobParts
			if (added >= span) {
				break;
			}

			const size = ArrayBuffer.isView(part) ? part.byteLength : part.size;
			if (relativeStart && size <= relativeStart) {
				// Skip the beginning and change the relative
				// start & end position as we skip the unwanted parts
				relativeStart -= size;
				relativeEnd -= size;
			} else {
				let chunk;
				if (ArrayBuffer.isView(part)) {
					chunk = part.subarray(
						relativeStart,
						Math.min(size, relativeEnd)
					);
					added += chunk.byteLength;
				} else {
					chunk = part.slice(
						relativeStart,
						Math.min(size, relativeEnd)
					);
					added += chunk.size;
				}
				relativeEnd -= size;
				blobParts.push(chunk);
				relativeStart = 0; // All next sequential parts should start at 0
			}
		}

		const blob = new Blob([], { type: `${type}` });
		blob.#size = span;
		blob.#parts = blobParts;

		return blob;
	}
}

Object.defineProperties(Blob.prototype, {
	size: { enumerable: true },
	type: { enumerable: true },
	slice: { enumerable: true },
});

def('Blob', Blob);
