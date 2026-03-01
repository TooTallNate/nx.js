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
		let callCount = 0;
		let totalIn = 0;
		let totalOut = 0;
		super({
			async transform(chunk, controller) {
				callCount++;
				totalIn += chunk.byteLength;
				const b: ArrayBuffer = await $.decompressWrite(h, chunk);
				totalOut += b.byteLength;
				if (callCount <= 5 || callCount % 500 === 0) {
					console.debug(
						`[DS transform #${callCount}] in=${chunk.byteLength} out=${b.byteLength} totalIn=${totalIn} totalOut=${totalOut}`,
					);
				}
				if (b.byteLength > 0) {
					controller.enqueue(new Uint8Array(b));
				}
			},
			async flush(controller) {
				console.debug(
					`[DS flush] calls=${callCount} totalIn=${totalIn} totalOut=${totalOut}`,
				);
				const b = await $.decompressFlush(h);
				if (b && b.byteLength > 0) {
					console.debug(`[DS flush] output=${b.byteLength}`);
					controller.enqueue(new Uint8Array(b));
				}
			},
		});
	}
}
def(DecompressionStream);
