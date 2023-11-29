/**
 * Based on `fetch-blob`.
 * MIT License.
 * Jimmy Wärting <https://jimmy.warting.se/opensource>
 */
import { createInternal, def } from '../utils';
import { encoder } from './text-encoder';
import { TextDecoder } from './text-decoder';
import type { BufferSource } from '../types';

interface BlobInternal {
	parts: (Blob | Uint8Array)[];
	type: string;
	size: number;
	endings: 'native' | 'transparent';
}

const _ = createInternal<Blob, BlobInternal>();

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

export type BlobPart = string | Blob | BufferSource;

export interface BlobPropertyBag {
	endings?: 'native' | 'transparent';
	type?: string;
}

/**
 * A file-like object of immutable, raw data. Blobs represent data that isn't
 * necessarily in a JavaScript-native format.
 */
export class Blob implements globalThis.Blob {
	/**
	 * @param blobParts - An array of BlobPart values that will be concatenated into a single Blob.
	 * @param options - An optional object that specifies the `Content-Type` and endings of the Blob.
	 */
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

		const internal: BlobInternal = {
			parts: [],
			size: 0,
			type: '',
			endings: 'transparent',
		};
		_.set(this, internal);

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
				internal.size += size;
				internal.parts.push(part);
			}
		}

		if (options.endings) {
			internal.endings = options.endings;
		}

		const type = options.type === undefined ? '' : String(options.type);
		if (/^[\x20-\x7E]*$/.test(type)) {
			internal.type = type;
		}
	}

	/**
	 * Returns the size of the Blob object, in bytes.
	 */
	get size() {
		return _(this).size;
	}

	/**
	 * Returns the MIME type of the Blob object.
	 */
	get type() {
		return _(this).type;
	}

	/**
	 * Returns a promise that resolves with a string representation of the Blob object.
	 */
	async text(): Promise<string> {
		// More optimized than using this.arrayBuffer()
		// that requires twice as much ram
		const decoder = new TextDecoder();
		let str = '';
		const parts = _(this).parts;
		for await (const part of toIterator(parts, false)) {
			str += decoder.decode(part, { stream: true });
		}
		// Remaining
		str += decoder.decode();
		return str;
	}

	/**
	 * Returns a promise that resolves with an ArrayBuffer representing the Blob's data.
	 */
	async arrayBuffer(): Promise<ArrayBuffer> {
		const data = new Uint8Array(this.size);
		let offset = 0;
		const parts = _(this).parts;
		for await (const chunk of toIterator(parts, false)) {
			data.set(chunk, offset);
			offset += chunk.length;
		}

		return data.buffer;
	}

	/**
	 * Returns a stream that can be used to read the contents of the Blob.
	 */
	stream() {
		const parts = _(this).parts;
		const it = toIterator(parts, true);

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

	/**
	 * Returns a new Blob object containing the data in the specified range of bytes of the source Blob.
	 *
	 * @param start - The start byte index.
	 * @param end - The end byte index.
	 * @param type - The content type of the new Blob.
	 */
	slice(start: number = 0, end: number = this.size, type: string = '') {
		const { size } = this;

		let relativeStart =
			start < 0 ? Math.max(size + start, 0) : Math.min(start, size);
		let relativeEnd =
			end < 0 ? Math.max(size + end, 0) : Math.min(end, size);

		const span = Math.max(relativeEnd - relativeStart, 0);
		const parts = _(this).parts;
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
		const internal = _(blob);
		internal.size = span;
		internal.parts = blobParts;

		return blob;
	}
}

Object.defineProperties(Blob.prototype, {
	size: { enumerable: true },
	type: { enumerable: true },
	slice: { enumerable: true },
});

def('Blob', Blob);
