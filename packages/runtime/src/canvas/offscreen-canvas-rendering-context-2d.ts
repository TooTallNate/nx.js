import { $ } from '../$';
import { ImageData } from './image-data';
import { createInternal, assertInternalConstructor, def } from '../utils';
import type { OffscreenCanvas } from './offscreen-canvas';

interface OffscreenCanvasRenderingContext2DInternal {
	canvas: OffscreenCanvas;
}

const _ = createInternal<
	OffscreenCanvasRenderingContext2D,
	OffscreenCanvasRenderingContext2DInternal
>();

export class OffscreenCanvasRenderingContext2D {
	constructor() {
		assertInternalConstructor(arguments);
		const canvas = arguments[1];
		const ctx = $.canvasContext2dNew(canvas);
		Object.setPrototypeOf(ctx, OffscreenCanvasRenderingContext2D.prototype);
		_.set(ctx, { canvas });
		return ctx;
	}

	/**
	 * A read-only reference to the {@link OffscreenCanvas | `OffscreenCanvas`} object that is associated with the context.
	 */
	get canvas() {
		return _(this).canvas;
	}

	/**
	 * Specifies the alpha (transparency) value that is applied to shapes and images before they are drawn onto the canvas.
	 *
	 * Value is between `0.0` (fully transparent) and `1.0` (fully opaque), inclusive. The default value is `1.0`. Values outside that range, including `Infinity` and `NaN`, will not be set, and `globalAlpha` will retain its previous value.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/globalAlpha
	 */
	declare globalAlpha: number;

	/**
	 * Specifies the miter limit ratio, in coordinate space units. Zero, negative, `Infinity`, and `NaN` values are ignored. The default value is `10.0`.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/miterLimit
	 */
	declare miterLimit: number;

	/**
	 * Draws a rectangle that is filled according to the current `fillStyle`.
	 *
	 * This method draws directly to the canvas without modifying the current path,
	 * so any subsequent `fill()` or `stroke()` calls will have no effect on it.
	 *
	 * @param x The x-axis coordinate of the rectangle's starting point.
	 * @param y The y-axis coordinate of the rectangle's starting point.
	 * @param width The rectangle's width. Positive values are to the right, and negative to the left.
	 * @param height The rectangle's height. Positive values are down, and negative are up.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/fillRect
	 */
	fillRect(x: number, y: number, width: number, height: number): void {
		// stub
	}

	getImageData(
		sx: number,
		sy: number,
		sw: number,
		sh: number,
		settings?: ImageDataSettings | undefined
	): ImageData {
		return new ImageData(
			new Uint8ClampedArray(
				$.canvasContext2dGetImageData(this, sx, sy, sw, sh)
			),
			sw,
			sh,
			settings
		);
	}
}
$.canvasContext2dInitClass(OffscreenCanvasRenderingContext2D);
def('OffscreenCanvasRenderingContext2D', OffscreenCanvasRenderingContext2D);
