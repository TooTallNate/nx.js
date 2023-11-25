import { $ } from '../$';
import { ImageData } from './image-data';
import { createInternal, assertInternalConstructor, def } from '../utils';
import type { Screen } from '../screen';
import type { CanvasLineCap, CanvasLineJoin } from '../types';

interface CanvasRenderingContext2DInternal {
	canvas: Screen;
}

const _ = createInternal<
	CanvasRenderingContext2D,
	CanvasRenderingContext2DInternal
>();

export class CanvasRenderingContext2D {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		const canvas: Screen = arguments[1];
		const ctx = $.canvasContext2dNew(canvas);
		Object.setPrototypeOf(ctx, CanvasRenderingContext2D.prototype);
		_.set(ctx, { canvas });
		return ctx;
	}

	/**
	 * A read-only reference to the {@link Screen | `Canvas`} object
	 * that is associated with the context.
	 */
	get canvas() {
		return _(this).canvas;
	}

	/**
	 * Specifies the alpha (transparency) value that is applied to shapes and images
	 * before they are drawn onto the canvas.
	 *
	 * Value is between `0.0` (fully transparent) and `1.0` (fully opaque), inclusive.
	 * Values outside that range, including `Infinity` and `NaN`, will not be set, and
	 * `globalAlpha` will retain its previous value.
	 *
	 * @default `1.0`
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/globalAlpha
	 */
	declare globalAlpha: number;

	/**
	 * Determines the shape used to draw the end points of lines.
	 *
	 * @default `"butt"`
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/lineCap
	 */
	declare lineCap: CanvasLineCap;

	/**
	 * Determines the line dash offset (or "phase").
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/lineDashOffset
	 */
	declare lineDashOffset: number;

	/**
	 * Determines the shape used to join two line segments where they meet.
	 *
	 * This property has no effect wherever two connected segments have the same
	 * direction, because no joining area will be added in this case. Degenerate
	 * segments with a length of zero (i.e. with all endpoints and control points
	 * at the exact same position) are also ignored.
	 *
	 * @default `"miter"`
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/lineJoin
	 */
	declare lineJoin: CanvasLineJoin;

	/**
	 * Determines the thickness of lines.
	 *
	 * @default `1.0`
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/lineWidth
	 */
	declare lineWidth: number;

	/**
	 * Specifies the miter limit ratio, in coordinate space units. Zero, negative,
	 * `Infinity`, and `NaN` values are ignored.
	 *
	 * @default `10.0`
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/miterLimit
	 */
	declare miterLimit: number;

	/**
	 * Saves the entire state of the canvas by pushing the current state onto a stack.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/save
	 */
	restore(): void {
		// stub
	}

	/**
	 * Restores the most recently saved canvas state by popping the top entry in the
	 * drawing state stack. If there is no saved state, this method does nothing.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/restore
	 */
	save(): void {
		// stub
	}

	/**
	 * Adds the specified angle of rotation to the transformation matrix.
	 *
	 * @param angle The rotation angle, clockwise in radians. You can use `degree * Math.PI / 180` to calculate a radian from a degree.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/rotate
	 */
	rotate(angle: number): void {
		// stub
	}

	/**
	 * Adds a scaling transformation to the canvas units horizontally and/or vertically.
	 *
	 * By default, one unit on the canvas is exactly one pixel. A scaling transformation
	 * modifies this behavior. For instance, a scaling factor of 0.5 results in a unit
	 * size of 0.5 pixels; shapes are thus drawn at half the normal size. Similarly, a
	 * scaling factor of 2.0 increases the unit size so that one unit becomes two pixels;
	 * shapes are thus drawn at twice the normal size.
	 *
	 * @param x Scaling factor in the horizontal direction. A negative value flips pixels across the vertical axis. A value of `1` results in no horizontal scaling.
	 * @param y Scaling factor in the vertical direction. A negative value flips pixels across the horizontal axis. A value of `1` results in no vertical scaling.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/scale
	 */
	scale(x: number, y: number): void {
		// stub
	}

	/**
	 * Retrieves the current transformation matrix being applied to the context.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/getTransform
	 */
	getTransform(): DOMMatrix {
		return new DOMMatrix($.canvasContext2dGetTransform(this));
	}

	/**
	 * Multiplies the current transformation with the matrix described by the arguments
	 * of this method. This lets you scale, rotate, translate (move), and skew the context.
	 *
	 * @param a (`m11`) The cell in the first row and first column of the matrix.
	 * @param b (`m12`) The cell in the second row and first column of the matrix.
	 * @param c (`m21`) The cell in the first row and second column of the matrix.
	 * @param d (`m22`) The cell in the second row and second column of the matrix.
	 * @param e (`m41`) The cell in the first row and third column of the matrix.
	 * @param f (`m42`) The cell in the second row and third column of the matrix.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/transform
	 */
	transform(
		a: number,
		b: number,
		c: number,
		d: number,
		e: number,
		f: number
	): void {
		// stub
	}

	/**
	 * Adds a translation transformation to the current matrix by moving the canvas
	 * and its origin `x` units horizontally and `y` units vertically on the grid.
	 *
	 * @param x Distance to move in the horizontal direction. Positive values are to the right, and negative to the left.
	 * @param y Distance to move in the vertical direction. Positive values are down, and negative are up.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/translate
	 */
	translate(x: number, y: number): void {
		// stub
	}

	/**
	 * Resets the current transform to the identity matrix.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/resetTransform
	 */
	resetTransform(): void {
		// stub
	}

	/**
	 * Gets the current line dash pattern.
	 *
	 * @returns An `Array` of numbers that specify distances to alternately draw a line and a gap (in coordinate space units). If the number, when setting the elements, is odd, the elements of the array get copied and concatenated. For example, setting the line dash to `[5, 15, 25]` will result in getting back `[5, 15, 25, 5, 15, 25]`.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/getLineDash
	 */
	getLineDash(): number[] {
		throw new Error('stub');
	}

	/**
	 * Sets the line dash pattern used when stroking lines. It uses an array of values that specify alternating lengths of lines and gaps which describe the pattern.
	 *
	 * @note To return to using solid lines, set the line dash list to an empty array.
	 * @param segments An `Array` of numbers that specify distances to alternately draw a line and a gap (in coordinate space units). If the number of elements in the array is odd, the elements of the array get copied and concatenated. For example, `[5, 15, 25]` will become `[5, 15, 25, 5, 15, 25]`. If the array is empty, the line dash list is cleared and line strokes return to being solid.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/setLineDash
	 */
	setLineDash(segments: number[]): void {
		// stub
	}

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

	/**
	 * Returns an {@link ImageData | `ImageData`} object representing the underlying pixel
	 * data for a specified portion of the canvas.
	 *
	 * This method is not affected by the canvas's transformation matrix. If the specified
	 * rectangle extends outside the bounds of the canvas, the pixels outside the canvas
	 * are transparent black in the returned `ImageData` object.
	 *
	 * @param sx The x-axis coordinate of the top-left corner of the rectangle from which the ImageData will be extracted.
	 * @param sy The y-axis coordinate of the top-left corner of the rectangle from which the ImageData will be extracted.
	 * @param sw The width of the rectangle from which the ImageData will be extracted. Positive values are to the right, and negative to the left.
	 * @param sh The height of the rectangle from which the ImageData will be extracted. Positive values are down, and negative are up.
	 * @returns An `ImageData` object containing the image data for the rectangle of the canvas specified.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/getImageData
	 */
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
$.canvasContext2dInitClass(CanvasRenderingContext2D);
def('CanvasRenderingContext2D', CanvasRenderingContext2D);
