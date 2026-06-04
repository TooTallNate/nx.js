import { $ } from '../$';
import type { DOMMatrix2DInit } from '../dommatrix';
import type { DOMPointInit } from '../dompoint';
import { def, proto, stub } from '../utils';

/**
 * Declares a path that can then be used on a {@link CanvasRenderingContext2D | `CanvasRenderingContext2D`} object.
 * The path methods of the `CanvasRenderingContext2D` interface are also present on this interface, which gives you
 * the convenience of being able to retain and replay your path whenever desired.
 *
 * Backed by a native Skia `SkPath` (built in user space); the canvas transform
 * is applied when the path is used via `ctx.fill()` / `stroke()` / `clip()` /
 * `isPointInPath()`. The method implementations live in the native layer
 * (`source/path2d.cc`); the bodies below are `stub()` placeholders that the
 * native `path2dInitClass` overwrites on the prototype — they exist only to
 * carry the TypeScript types and the TSDoc.
 *
 * @see https://developer.mozilla.org/docs/Web/API/Path2D
 */
export class Path2D implements globalThis.Path2D {
	constructor(path?: Path2D | string) {
		return proto($.path2dNew(path), Path2D);
	}

	/**
	 * Adds to the path the path given by the argument, optionally transformed
	 * by `transform`.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Path2D/addPath
	 */
	addPath(path: Path2D, transform?: DOMMatrix2DInit): void {
		stub();
	}

	/**
	 * Causes the point of the pen to move back to the start of the current
	 * sub-path.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/closePath
	 */
	closePath(): void {
		stub();
	}

	/**
	 * Moves the starting point of a new sub-path to the `(x, y)` coordinates.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/moveTo
	 */
	moveTo(x: number, y: number): void {
		stub();
	}

	/**
	 * Connects the last point in the current sub-path to the `(x, y)`
	 * coordinates with a straight line.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/lineTo
	 */
	lineTo(x: number, y: number): void {
		stub();
	}

	/**
	 * Adds a cubic Bézier curve to the current sub-path.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/bezierCurveTo
	 */
	bezierCurveTo(
		cp1x: number,
		cp1y: number,
		cp2x: number,
		cp2y: number,
		x: number,
		y: number,
	): void {
		stub();
	}

	/**
	 * Adds a quadratic Bézier curve to the current sub-path.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/quadraticCurveTo
	 */
	quadraticCurveTo(cpx: number, cpy: number, x: number, y: number): void {
		stub();
	}

	/**
	 * Adds a circular arc to the current sub-path.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/arc
	 */
	arc(
		x: number,
		y: number,
		radius: number,
		startAngle: number,
		endAngle: number,
		counterclockwise?: boolean,
	): void {
		stub();
	}

	/**
	 * Adds a circular arc to the current sub-path, using the given control
	 * points and radius.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/arcTo
	 */
	arcTo(x1: number, y1: number, x2: number, y2: number, radius: number): void {
		stub();
	}

	/**
	 * Adds an elliptical arc to the current sub-path.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/ellipse
	 */
	ellipse(
		x: number,
		y: number,
		radiusX: number,
		radiusY: number,
		rotation: number,
		startAngle: number,
		endAngle: number,
		counterclockwise?: boolean,
	): void {
		stub();
	}

	/**
	 * Adds a rectangle to the current path.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/rect
	 */
	rect(x: number, y: number, w: number, h: number): void {
		stub();
	}

	/**
	 * Adds a rounded rectangle to the current path.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasRenderingContext2D/roundRect
	 */
	roundRect(
		x: number,
		y: number,
		w: number,
		h: number,
		radii?: number | DOMPointInit | (number | DOMPointInit)[],
	): void {
		stub();
	}
}
$.path2dInitClass(Path2D);
def(Path2D);
