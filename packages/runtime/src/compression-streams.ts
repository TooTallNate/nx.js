import { $ } from './$';
import { kNativeRead, kNativeSetup } from './polyfills/streams';
import { def } from './utils';

/**
 * Compression formats supported by {@link CompressionStream | `CompressionStream`} and {@link DecompressionStream | `DecompressionStream`}.
 */
export type CompressionFormat = 'deflate' | 'deflate-raw' | 'gzip' | 'zstd';

/**
 * @see https://developer.mozilla.org/docs/Web/API/CompressionStream
 */
export class CompressionStream
	extends TransformStream<Uint8Array, Uint8Array>
	implements globalThis.CompressionStream
{
	/** @internal */
	[kNativeSetup]?: (
		source: ReadableStream<Uint8Array>,
	) => ReadableStream<Uint8Array>;

	constructor(format: CompressionFormat) {
		const h = $.compressNew(format);
		super({
			async transform(chunk, controller) {
				const b = await $.compressWrite(h, chunk);
				controller.enqueue(new Uint8Array(b));
			},
			async flush(controller) {
				const b = await $.compressFlush(h);
				if (b) {
					controller.enqueue(new Uint8Array(b));
				}
			},
		});
		this[kNativeSetup] = (source) => {
			const nativeRead = (source as any)[kNativeRead] as
				| (() => Promise<IteratorResult<Uint8Array, undefined>>)
				| undefined;
			const reader = nativeRead ? null : source.getReader();
			const readNext = nativeRead ? nativeRead : () => reader!.read();

			let isDone = false;
			return new ReadableStream<Uint8Array>({
				async pull(controller) {
					while (!isDone) {
						const result = await readNext();
						if (result.done) {
							const flushed = await $.compressFlush(h);
							if (flushed && flushed.byteLength > 0) {
								controller.enqueue(new Uint8Array(flushed));
							}
							controller.close();
							isDone = true;
							return;
						}
						const b = await $.compressWrite(h, result.value!);
						if (b.byteLength > 0) {
							controller.enqueue(new Uint8Array(b));
							return;
						}
					}
					controller.close();
				},
				cancel() {
					if (reader) reader.cancel();
				},
			});
		};
	}
}
def(CompressionStream);

/**
 * @see https://developer.mozilla.org/docs/Web/API/DecompressionStream
 */
export class DecompressionStream
	extends TransformStream<Uint8Array, Uint8Array>
	implements globalThis.DecompressionStream
{
	/** @internal */
	[kNativeSetup]?: (
		source: ReadableStream<Uint8Array>,
	) => ReadableStream<Uint8Array>;

	constructor(format: CompressionFormat) {
		const h = $.decompressNew(format);
		super({
			async transform(chunk, controller) {
				if (format === 'zstd') {
					let offset = 0;
					while (offset < chunk.length) {
						const b = await $.decompressWrite(h, chunk.subarray(offset));
						if (b.byteLength > 0) {
							controller.enqueue(new Uint8Array(b));
						}
						const consumed = (h as any).inputConsumed as number;
						offset += consumed;
						if (consumed === 0) break;
					}
				} else {
					const b = await $.decompressWrite(h, chunk);
					if (b.byteLength > 0) {
						controller.enqueue(new Uint8Array(b));
					}
				}
			},
			async flush(controller) {
				const b = await $.decompressFlush(h);
				if (b) {
					controller.enqueue(new Uint8Array(b));
				}
			},
		});
		this[kNativeSetup] = (source) => {
			const nativeRead = (source as any)[kNativeRead] as
				| (() => Promise<IteratorResult<Uint8Array, undefined>>)
				| undefined;
			const reader = nativeRead ? null : source.getReader();
			const readNext = nativeRead ? nativeRead : () => reader!.read();

			let isDone = false;
			return new ReadableStream<Uint8Array>({
				async pull(controller) {
					while (!isDone) {
						const result = await readNext();
						if (result.done) {
							const flushed = await $.decompressFlush(h);
							if (flushed && flushed.byteLength > 0) {
								controller.enqueue(new Uint8Array(flushed));
							}
							controller.close();
							isDone = true;
							return;
						}
						const chunk = result.value!;
						let enqueued = false;
						if (format === 'zstd') {
							let offset = 0;
							while (offset < chunk.length) {
								const b = await $.decompressWrite(h, chunk.subarray(offset));
								if (b.byteLength > 0) {
									controller.enqueue(new Uint8Array(b));
									enqueued = true;
								}
								const consumed = (h as any).inputConsumed as number;
								offset += consumed;
								if (consumed === 0) break;
							}
						} else {
							const b = await $.decompressWrite(h, chunk);
							if (b.byteLength > 0) {
								controller.enqueue(new Uint8Array(b));
								enqueued = true;
							}
						}
						if (enqueued) return;
					}
					controller.close();
				},
				cancel() {
					if (reader) reader.cancel();
				},
			});
		};
	}
}
def(DecompressionStream);
