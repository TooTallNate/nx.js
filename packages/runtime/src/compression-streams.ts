import { $ } from './$';
import { def } from './utils';
import {
	kNativeDecompressSetup,
	type NativeFileSource,
} from './polyfills/streams';

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
		// Transparent fast path: when piped FROM a native file source (see
		// FsFile.stream + ReadableStream.pipeThrough override), bypass the
		// polyfill pipe and stream directly out of the fused native
		// read+decompress op — one thread-pool dispatch per chunk, fixed
		// reused buffers, no per-chunk promise churn or realloc spikes.
		Object.defineProperty(this, kNativeDecompressSetup, {
			value: (src: NativeFileSource): ReadableStream<Uint8Array> => {
				let handle: ReturnType<typeof $.decompressFileNew> | null = null;
				// Larger per-pull output in the application regime (fewer
				// thread-pool dispatches), conservative in the applet regime to
				// stay within the tight native-heap budget. AppletType.Application === 0.
				const outCap =
					$.appletGetAppletType() === 0
						? 8 * 1024 * 1024
						: 1024 * 1024;
				return new ReadableStream<Uint8Array>({
					type: 'bytes',
					async pull(controller) {
						if (!handle) {
							handle = $.decompressFileNew(
								format,
								src.path,
								src.start,
								src.end,
								outCap,
							);
						}
						const b = await $.decompressFilePull(handle);
						if (b === null) {
							controller.close();
						} else {
							controller.enqueue(new Uint8Array(b));
						}
					},
				});
			},
			enumerable: false,
		});
	}
}
def(DecompressionStream);
