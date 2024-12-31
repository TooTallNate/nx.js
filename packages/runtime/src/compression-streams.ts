import { $ } from './$';
import { def } from './utils';

export type CompressionFormat = 'deflate' | 'deflate-raw' | 'gzip' | 'zstd';

/**
 * @see https://developer.mozilla.org/en-US/docs/Web/API/CompressionStream
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
 * @see https://developer.mozilla.org/en-US/docs/Web/API/DecompressionStream
 */
export class DecompressionStream
	extends TransformStream<Uint8Array, Uint8Array>
	implements globalThis.DecompressionStream
{
	constructor(format: CompressionFormat) {
		const h = $.decompressNew(format);
		super({
			async transform(chunk, controller) {
				const b = await $.decompressWrite(h, chunk);
				controller.enqueue(new Uint8Array(b));
			},
			async flush(controller) {
				const b = await $.decompressFlush(h);
				if (b) {
					controller.enqueue(new Uint8Array(b));
				}
			},
		});
	}
}
def(DecompressionStream);
