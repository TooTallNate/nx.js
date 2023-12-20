import toPx = require('to-px/index.js');
import colorRgba = require('color-rgba');
import parseCssFont from 'parse-css-font';
import { $ } from '../$';
import { ImageData } from './image-data';
import {
	createInternal,
	assertInternalConstructor,
	def,
	rgbaToString,
	stub,
	returnOnThrow,
} from '../utils';
import { isDomPointInit, type DOMPointInit } from '../dompoint';
import { addSystemFont, findFont, fonts } from '../font/font-face-set';
import type { Path2D } from './path2d';
import type { OffscreenCanvas } from './offscreen-canvas';
import type {
	CanvasFillRule,
	CanvasLineCap,
	CanvasLineJoin,
	CanvasImageSource,
	CanvasTextAlign,
	CanvasTextBaseline,
	GlobalCompositeOperation,
	ImageSmoothingQuality,
	TextMetrics,
} from '../types';

interface OffscreenCanvasRenderingContext2DInternal {
	canvas: OffscreenCanvas;
}

const _ = createInternal<
	OffscreenCanvasRenderingContext2D,
	OffscreenCanvasRenderingContext2DInternal
>();

export class OffscreenCanvasRenderingContext2D {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		const canvas: OffscreenCanvas = arguments[1];
		const ctx = $.canvasContext2dNew(canvas);
		Object.setPrototypeOf(ctx, OffscreenCanvasRenderingContext2D.prototype);
		_.set(ctx, { canvas });
		ctx.font = '10px system-ui';
		return ctx;
	}

	/**
	 * A read-only reference to the {@link OffscreenCanvas | `OffscreenCanvas`} object
	 * that is associated with the context.
	 */
	get canvas() {
		return _(this).canvas;
	}

	/**
	 * Specifies the current text style to use when drawing text.
	 * This string uses the same syntax as the
	 * [CSS font](https://developer.mozilla.org/docs/Web/CSS/font) specifier.
	 *
	 * @default "10px system-ui"
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/font
	 */
	get font(): string {
		return $.canvasContext2dGetFont(this);
	}
	set font(v: string) {
		if (!v || this.font === v) return;

		const parsed = returnOnThrow(parseCssFont, v);
		if (parsed instanceof Error) {
			return;
		}
		if ('system' in parsed) {
			// "system" fonts are not supported
			return;
		}
		if (typeof parsed.size !== 'string') {
			// Missing required font size
			return;
		}
		if (!parsed.family) {
			// Missing required font name
			return;
		}
		const px = toPx(parsed.size);
		if (typeof px !== 'number') {
			// Invalid font size
			return;
		}
		let font = findFont(fonts, parsed);
		if (!font) {
			if (parsed.family.includes('system-ui')) {
				font = addSystemFont(fonts);
			} else {
				return;
			}
		}
		$.canvasContext2dSetFont(this, font, px, v);
	}

	/**
	 * Strokes (outlines) the current or given path with the current stroke style.
	 *
	 * @param path A {@link Path2D | `Path2D`} path to stroke.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/stroke
	 */
	stroke(path?: Path2D): void {
		stub();
	}

	/**
	 * Fills the current or given path with the current {@link OffscreenCanvasRenderingContext2D.fillStyle | `fillStyle`}.
	 *
	 * @param fillRule
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/fill
	 */
	fill(fillRule?: CanvasFillRule): void;
	fill(path: Path2D, fillRule?: CanvasFillRule): void;
	fill(
		fillRuleOrPath?: CanvasFillRule | Path2D,
		fillRule?: CanvasFillRule
	): void {
		stub();
	}

	/**
	 * Specifies the color, gradient, or pattern to use inside shapes.
	 *
	 * @default "#000" (black)
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/fillStyle
	 */
	get fillStyle(): string {
		return rgbaToString($.canvasContext2dGetFillStyle(this));
	}
	set fillStyle(v: string) {
		if (typeof v === 'string') {
			const parsed = colorRgba(v);
			if (!parsed || parsed.length !== 4) {
				return;
			}
			$.canvasContext2dSetFillStyle(this, ...parsed);
		} else {
			throw new Error('CanvasGradient/CanvasPattern not implemented.');
		}
	}

	/**
	 * Specifies the color, gradient, or pattern to use for the strokes (outlines) around shapes.
	 *
	 * @default "#000" (black)
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/strokeStyle
	 */
	get strokeStyle(): string {
		return rgbaToString($.canvasContext2dGetStrokeStyle(this));
	}
	set strokeStyle(v: string) {
		if (typeof v === 'string') {
			const parsed = colorRgba(v);
			if (!parsed || parsed.length !== 4) {
				return;
			}
			$.canvasContext2dSetStrokeStyle(this, ...parsed);
		} else {
			throw new Error('CanvasGradient/CanvasPattern not implemented.');
		}
	}

	/**
	 * Specifies the alpha (transparency) value that is applied to shapes and images
	 * before they are drawn onto the canvas.
	 *
	 * Value is between `0.0` (fully transparent) and `1.0` (fully opaque), inclusive.
	 * Values outside that range, including `Infinity` and `NaN`, will not be set, and
	 * `globalAlpha` will retain its previous value.
	 *
	 * @default 1.0
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/globalAlpha
	 */
	declare globalAlpha: number;

	/**
	 * Specifies the type of compositing operation to apply when drawing new shapes.
	 *
	 * @default "source-over"
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/globalCompositeOperation
	 */
	declare globalCompositeOperation: GlobalCompositeOperation;

	/**
	 * Determines whether scaled images are smoothed (`true`) or not (`false`).
	 *
	 * @default true
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/imageSmoothingEnabled
	 */
	declare imageSmoothingEnabled: boolean;

	/**
	 * Determines the quality of image smoothing.
	 *
	 * @default "low"
	 * @note For this property to have an effect, {@link OffscreenCanvasRenderingContext2D.imageSmoothingEnabled | `imageSmoothingEnabled`} must be true.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/imageSmoothingQuality
	 */
	declare imageSmoothingQuality: ImageSmoothingQuality;

	/**
	 * Determines the shape used to draw the end points of lines.
	 *
	 * @default "butt"
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
	 * @default "miter"
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/lineJoin
	 */
	declare lineJoin: CanvasLineJoin;

	/**
	 * Determines the thickness of lines.
	 *
	 * @default 1.0
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/lineWidth
	 */
	declare lineWidth: number;

	/**
	 * Specifies the miter limit ratio, in coordinate space units. Zero, negative,
	 * `Infinity`, and `NaN` values are ignored.
	 *
	 * @default 10.0
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/miterLimit
	 */
	declare miterLimit: number;

	/**
	 * Specifies the current text alignment used when drawing text.
	 *
	 * The alignment is relative to the `x` value of the {@link CanvasRenderingContext2D.fillText | `fillText()`} /
	 * {@link CanvasRenderingContext2D.strokeText | `strokeText()`} methods.
	 * For example, if `textAlign` is `"center"`, then the text's left
	 * edge will be at `x - (textWidth / 2)`.
	 *
	 * @default "start"
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/textAlign
	 */
	declare textAlign: CanvasTextAlign;

	/**
	 * Specifies the current text baseline used when drawing text.
	 *
	 * @default "alphabetic"
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/textBaseline
	 */
	declare textBaseline: CanvasTextBaseline;

	/**
	 * Starts a new path by emptying the list of sub-paths.
	 * Call this method when you want to create a new path.
	 *
	 * @note To create a new sub-path (i.e. one matching the current canvas state),
	 * you can use `CanvasRenderingContext2D.moveTo()`.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/beginPath
	 */
	beginPath(): void {
		stub();
	}

	/**
	 * Attempts to add a straight line from the current point to the start of
	 * the current sub-path. If the shape has already been closed or has only
	 * one point, this function does nothing.
	 *
	 * This method doesn't draw anything to the canvas directly. You can render
	 * the path using the stroke() or fill() methods.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/closePath
	 */
	closePath(): void {
		stub();
	}

	/**
	 * Saves the entire state of the canvas by pushing the current state onto a stack.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/save
	 */
	restore(): void {
		stub();
	}

	/**
	 * Restores the most recently saved canvas state by popping the top entry in the
	 * drawing state stack. If there is no saved state, this method does nothing.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/restore
	 */
	save(): void {
		stub();
	}

	/**
	 * Adds the specified angle of rotation to the transformation matrix.
	 *
	 * @param angle The rotation angle, clockwise in radians. You can use `degree * Math.PI / 180` to calculate a radian from a degree.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/rotate
	 */
	rotate(angle: number): void {
		stub();
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
		stub();
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
		stub();
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
		stub();
	}

	/**
	 * Resets the current transform to the identity matrix.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/resetTransform
	 */
	resetTransform(): void {
		stub();
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
		stub();
	}

	/**
	 * Erases the pixels in a rectangular area by setting them to transparent black.
	 *
	 * @param x The x-axis coordinate of the rectangle's starting point.
	 * @param y The y-axis coordinate of the rectangle's starting point.
	 * @param width The rectangle's width. Positive values are to the right, and negative to the left.
	 * @param height The rectangle's height. Positive values are down, and negative are up.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/clearRect
	 */
	clearRect(x: number, y: number, width: number, height: number): void {
		stub();
	}

	/**
	 * Draws a rectangle that is filled according to the current
	 * {@link OffscreenCanvasRenderingContext2D.fillStyle | `fillStyle`}.
	 *
	 * This method draws directly to the canvas without modifying the current path,
	 * so any subsequent {@link OffscreenCanvasRenderingContext2D.fill | `fill()`} or
	 * {@link OffscreenCanvasRenderingContext2D.stroke | `stroke()`} calls will have no effect on it.
	 *
	 * @param x The x-axis coordinate of the rectangle's starting point.
	 * @param y The y-axis coordinate of the rectangle's starting point.
	 * @param width The rectangle's width. Positive values are to the right, and negative to the left.
	 * @param height The rectangle's height. Positive values are down, and negative are up.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/fillRect
	 */
	fillRect(x: number, y: number, width: number, height: number): void {
		stub();
	}

	/**
	 * Draws a rectangle that is stroked (outlined) according to the current
	 * {@link OffscreenCanvasRenderingContext2D.strokeStyle | `strokeStyle`} and other context settings.
	 *
	 * This method draws directly to the canvas without modifying the current path,
	 * so any subsequent {@link OffscreenCanvasRenderingContext2D.fill | `fill()`} or
	 * {@link OffscreenCanvasRenderingContext2D.stroke | `stroke()`} calls will have no effect on it.
	 *
	 * @param x The x-axis coordinate of the rectangle's starting point.
	 * @param y The y-axis coordinate of the rectangle's starting point.
	 * @param width The rectangle's width. Positive values are to the right, and negative to the left.
	 * @param height The rectangle's height. Positive values are down, and negative are up.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/strokeRect
	 */
	strokeRect(x: number, y: number, width: number, height: number): void {
		stub();
	}

	createImageData(
		sw: number,
		sh: number,
		settings?: ImageDataSettings
	): ImageData;
	createImageData(imagedata: ImageData): ImageData;
	createImageData(
		sw: number | ImageData,
		sh?: number,
		settings?: ImageDataSettings
	): ImageData {
		let width: number;
		let height: number;
		if (typeof sw === 'number') {
			if (typeof sh !== 'number') {
				throw new TypeError(
					'OffscreenCanvasRenderingContext2D.createImageData: Argument 1 is not an object.'
				);
			}
			width = sw;
			height = sh;
		} else {
			width = sw.width;
			height = sw.height;
		}
		if (width <= 0 || height <= 0) {
			throw new TypeError(
				'OffscreenCanvasRenderingContext2D.createImageData: Invalid width or height'
			);
		}
		return new ImageData(width, height, settings);
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

	putImageData(imageData: ImageData, dx: number, dy: number): void;
	putImageData(
		imageData: ImageData,
		dx: number,
		dy: number,
		dirtyX?: number,
		dirtyY?: number,
		dirtyWidth?: number,
		dirtyHeight?: number
	): void {
		stub();
	}

	/**
	 * Draws an image onto the canvas.
	 *
	 * @param image The image to draw onto the canvas.
	 * @param dx The x-axis coordinate in the destination canvas at which to place the top-left corner of the source `image`.
	 * @param dy The y-axis coordinate in the destination canvas at which to place the top-left corner of the source `image`.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/drawImage
	 */
	drawImage(image: CanvasImageSource, dx: number, dy: number): void;
	/**
	 * Draws an image onto the canvas.
	 *
	 * @param image The image to draw onto the canvas.
	 * @param dx The x-axis coordinate in the destination canvas at which to place the top-left corner of the source `image`.
	 * @param dy The y-axis coordinate in the destination canvas at which to place the top-left corner of the source `image`.
	 * @param dWidth The width to draw the `image` in the destination canvas. This allows scaling of the drawn image.
	 * @param dHeight The height to draw the `image` in the destination canvas. This allows scaling of the drawn image.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/drawImage
	 */
	drawImage(
		image: CanvasImageSource,
		dx: number,
		dy: number,
		dWidth: number,
		dHeight: number
	): void;
	/**
	 * Draws an image onto the canvas.
	 *
	 * @param image The image to draw onto the canvas.
	 * @param sx The x-axis coordinate of the top left corner of the sub-rectangle of the source `image` to draw into the destination context.
	 * @param sy The y-axis coordinate of the top left corner of the sub-rectangle of the source `image` to draw into the destination context.
	 * @param sWidth The width of the sub-rectangle of the source `image` to draw into the destination context.
	 * @param sHeight The height of the sub-rectangle of the source `image` to draw into the destination context.
	 * @param dx The x-axis coordinate in the destination canvas at which to place the top-left corner of the source `image`.
	 * @param dy The y-axis coordinate in the destination canvas at which to place the top-left corner of the source `image`.
	 * @param dWidth The width to draw the `image` in the destination canvas. This allows scaling of the drawn image.
	 * @param dHeight The height to draw the `image` in the destination canvas. This allows scaling of the drawn image.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/drawImage
	 */
	drawImage(
		image: CanvasImageSource,
		sx: number,
		sy: number,
		sWidth: number,
		sHeight: number,
		dx: number,
		dy: number,
		dWidth: number,
		dHeight: number
	): void;
	drawImage(
		image: CanvasImageSource,
		dxOrSx: number,
		dyOrSy: number,
		dwOrSw?: number,
		dhOrSh?: number,
		dx?: number,
		dy?: number,
		dw?: number,
		dh?: number
	): void {
		stub();
	}

	lineTo(x: number, y: number): void {
		stub();
	}

	moveTo(x: number, y: number): void {
		stub();
	}

	rect(x: number, y: number, w: number, h: number): void {
		stub();
	}

	arc(
		x: number,
		y: number,
		radius: number,
		startAngle: number,
		endAngle: number,
		counterclockwise?: boolean
	): void {
		stub();
	}

	arcTo(
		x1: number,
		y1: number,
		x2: number,
		y2: number,
		radius: number
	): void {
		stub();
	}

	bezierCurveTo(
		cp1x: number,
		cp1y: number,
		cp2x: number,
		cp2y: number,
		x: number,
		y: number
	): void {
		stub();
	}

	quadraticCurveTo(cpx: number, cpy: number, x: number, y: number): void {
		stub();
	}

	ellipse(
		x: number,
		y: number,
		radiusX: number,
		radiusY: number,
		rotation: number,
		startAngle: number,
		endAngle: number,
		counterclockwise?: boolean
	): void {
		stub();
	}

	/**
	 * Implementation from https://github.com/nilzona/path2d-polyfill
	 *
	 * @note Currently does not support negative width / height values.
	 */
	roundRect(
		x: number,
		y: number,
		width: number,
		height: number,
		radii: number | DOMPointInit | Iterable<number | DOMPointInit> = 0
	): void {
		const r: number[] = (
			typeof radii === 'number' || isDomPointInit(radii)
				? [radii]
				: Array.from(radii)
		).map<number>((v) => {
			if (typeof v !== 'number') {
				// DOMPoint
				v = Math.sqrt(
					(v.x || 0) * (v.x || 0) + (v.y || 0) * (v.y || 0)
				);
			}
			if (v < 0) {
				throw new RangeError(`Radius value ${v} is negative.`);
			}
			return v;
		});

		// check for range error
		if (r.length === 0 || r.length > 4) {
			throw new RangeError(
				`${r.length} radii provided. Between one and four radii are necessary.`
			);
		}

		if (r.length === 1 && r[0] === 0) {
			return this.rect(x, y, width, height);
		}

		// set the corners
		// tl = top left radius
		// tr = top right radius
		// br = bottom right radius
		// bl = bottom left radius
		const minRadius = Math.min(width, height) / 2;
		let tr, br, bl;
		const tl = (tr = br = bl = Math.min(minRadius, r[0]));
		if (r.length === 2) {
			tr = bl = Math.min(minRadius, r[1]);
		}
		if (r.length === 3) {
			tr = bl = Math.min(minRadius, r[1]);
			br = Math.min(minRadius, r[2]);
		}
		if (r.length === 4) {
			tr = Math.min(minRadius, r[1]);
			br = Math.min(minRadius, r[2]);
			bl = Math.min(minRadius, r[3]);
		}

		// begin with closing current path
		this.closePath();

		// draw the rounded rectangle
		this.moveTo(x, y + height - bl);
		this.arcTo(x, y, x + tl, y, tl);
		this.arcTo(x + width, y, x + width, y + tr, tr);
		this.arcTo(x + width, y + height, x + width - br, y + height, br);
		this.arcTo(x, y + height, x, y + height - bl, bl);

		// move to rects control point for further path drawing
		this.moveTo(x, y);
	}

	/**
	 * Draws the outlines of the characters of the text string at the specified coordinates,
	 * stroking the string's characters with the current {@link CanvasRenderingContext2D.strokeStyle | `strokeStyle`}.
	 *
	 * @param text A string specifying the text string to render into the context.
	 * @param x The x-axis coordinate of the point at which to begin drawing the text, in pixels.
	 * @param y The y-axis coordinate of the baseline on which to begin drawing the text, in pixels.
	 * @param maxWidth The maximum number of pixels wide the text may be once rendered. If not specified, there is no limit to the width of the text.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/strokeText
	 */
	strokeText(text: string, x: number, y: number, maxWidth?: number): void {
		stub();
	}

	createConicGradient(
		startAngle: number,
		x: number,
		y: number
	): CanvasGradient {
		throw new Error('Method not implemented.');
	}
	createLinearGradient(
		x0: number,
		y0: number,
		x1: number,
		y1: number
	): CanvasGradient {
		throw new Error('Method not implemented.');
	}
	createPattern(
		image: CanvasImageSource,
		repetition: string | null
	): CanvasPattern | null {
		throw new Error('Method not implemented.');
	}
	createRadialGradient(
		x0: number,
		y0: number,
		r0: number,
		x1: number,
		y1: number,
		r1: number
	): CanvasGradient {
		throw new Error('Method not implemented.');
	}

	/**
	 * Draws a text string at the specified coordinates, filling the string's
	 * characters with the current {@link CanvasRenderingContext2D.fillStyle | `fillStyle`}.
	 *
	 * @param text A string specifying the text string to render into the context.
	 * @param x The x-axis coordinate of the point at which to begin drawing the text, in pixels.
	 * @param y The y-axis coordinate of the baseline on which to begin drawing the text, in pixels.
	 * @param maxWidth The maximum number of pixels wide the text may be once rendered. If not specified, there is no limit to the width of the text.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/fillText
	 */
	fillText(text: string, x: number, y: number, maxWidth?: number): void {
		stub();
	}

	/**
	 * Returns a `TextMetrics` object that contains information about
	 * the measured text (such as its width, for example).
	 *
	 * @param text The text string to measure.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/measureText
	 */
	measureText(text: string): TextMetrics {
		stub();
	}
}
$.canvasContext2dInitClass(OffscreenCanvasRenderingContext2D);
def('OffscreenCanvasRenderingContext2D', OffscreenCanvasRenderingContext2D);
