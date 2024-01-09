import { $ } from '../$';
import { assertInternalConstructor, def } from '../utils';
import type { ImageBitmapSource } from '../types';

/**
 * Represents a bitmap image which can be drawn to a `<canvas>` without undue latency.
 * It can be created from a variety of source objects using the
 * {@link createImageBitmap | `createImageBitmap()`} function.
 *
 * @see https://developer.mozilla.org/docs/Web/API/ImageBitmap
 */
export class ImageBitmap implements globalThis.ImageBitmap {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
	}

	/**
	 * Read-only property containing the height of the `ImageBitmap` in CSS pixels.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/ImageBitmap/height
	 */
	declare height: number;

	/**
	 * Read-only property containing the width of the `ImageBitmap` in CSS pixels.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/ImageBitmap/width
	 */
	declare width: number;

	/**
	 * Disposes of all graphical resources associated with the `ImageBitmap`.
	 * @see https://developer.mozilla.org/docs/Web/API/ImageBitmap/close
	 */
	close(): void {
		$.imageClose(this);
	}
}
$.imageInit(ImageBitmap);
def(ImageBitmap);

export interface ImageBitmapOptions {
	colorSpaceConversion?: ColorSpaceConversion;
	imageOrientation?: ImageOrientation;
	premultiplyAlpha?: PremultiplyAlpha;
	resizeHeight?: number;
	resizeQuality?: ResizeQuality;
	resizeWidth?: number;
}

/**
 * Creates a bitmap from a given source, optionally cropped to contain only
 * a portion of that source. This function accepts a variety of different
 * image sources, and returns a `Promise` which resolves to an {@link ImageBitmap}.
 *
 * @see https://developer.mozilla.org/docs/Web/API/createImageBitmap
 */
export function createImageBitmap(
	image: ImageBitmapSource,
	options?: ImageBitmapOptions
): Promise<ImageBitmap>;
export function createImageBitmap(
	image: ImageBitmapSource,
	sx: number,
	sy: number,
	sw: number,
	sh: number,
	options?: ImageBitmapOptions
): Promise<ImageBitmap>;
export async function createImageBitmap(
	image: ImageBitmapSource,
	optionsOrSx?: ImageBitmapOptions | number,
	sy?: number,
	sw?: number,
	sh?: number,
	options?: ImageBitmapOptions
): Promise<ImageBitmap> {
	throw new Error('Function not implemented');
}
def(createImageBitmap);
