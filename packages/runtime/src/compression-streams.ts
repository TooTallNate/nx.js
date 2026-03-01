import { $ } from './$';
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
	constructor(format: CompressionFormat) {
		const h = $.decompressNew(format);
		let transformCount = 0;
		super({
			async transform(chunk, controller) {
				const b = await $.decompressWrite(h, chunk);
				if (b.byteLength > 0) {
					controller.enqueue(new Uint8Array(b));
				}
				// Periodically trigger GC to free accumulated ArrayBuffers.
				// The web-streams-polyfill creates closures and promises per
				// chunk that QuickJS's refcount GC cannot always collect.
				// Running the cycle collector every N transforms prevents
				// unbounded memory growth during large streaming operations.
				if (++transformCount % 100 === 0) {
					$.gc();
				}
			},
			async flush(controller) {
				const b = await $.decompressFlush(h);
				if (b && b.byteLength > 0) {
					controller.enqueue(new Uint8Array(b));
				}
			},
		});
	}
}
def(DecompressionStream);
