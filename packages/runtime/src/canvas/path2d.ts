import { $ } from '../$';
import { def, proto } from '../utils';

/**
 * Declares a path that can then be used on a {@link CanvasRenderingContext2D | `CanvasRenderingContext2D`} object.
 * The path methods of the `CanvasRenderingContext2D` interface are also present on this interface, which gives you
 * the convenience of being able to retain and replay your path whenever desired.
 *
 * Backed by a native Skia `SkPath` (built in user space); the canvas transform
 * is applied when the path is used via `ctx.fill()` / `stroke()` / `clip()` /
 * `isPointInPath()`.
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
		$.path2dAddPath(this, path, transform);
	}

	closePath(): void {
		$.path2dClosePath(this);
	}

	moveTo(x: number, y: number): void {
		$.path2dMoveTo(this, x, y);
	}

	lineTo(x: number, y: number): void {
		$.path2dLineTo(this, x, y);
	}

	bezierCurveTo(
		cp1x: number,
		cp1y: number,
		cp2x: number,
		cp2y: number,
		x: number,
		y: number,
	): void {
		$.path2dBezierCurveTo(this, cp1x, cp1y, cp2x, cp2y, x, y);
	}

	quadraticCurveTo(cpx: number, cpy: number, x: number, y: number): void {
		$.path2dQuadraticCurveTo(this, cpx, cpy, x, y);
	}

	arc(
		x: number,
		y: number,
		radius: number,
		startAngle: number,
		endAngle: number,
		counterclockwise = false,
	): void {
		$.path2dArc(this, x, y, radius, startAngle, endAngle, counterclockwise);
	}

	arcTo(x1: number, y1: number, x2: number, y2: number, radius: number): void {
		$.path2dArcTo(this, x1, y1, x2, y2, radius);
	}

	ellipse(
		x: number,
		y: number,
		radiusX: number,
		radiusY: number,
		rotation: number,
		startAngle: number,
		endAngle: number,
		counterclockwise = false,
	): void {
		$.path2dEllipse(
			this,
			x,
			y,
			radiusX,
			radiusY,
			rotation,
			startAngle,
			endAngle,
			counterclockwise,
		);
	}

	rect(x: number, y: number, w: number, h: number): void {
		$.path2dRect(this, x, y, w, h);
	}

	roundRect(
		x: number,
		y: number,
		w: number,
		h: number,
		radii?: number | DOMPointInit | (number | DOMPointInit)[],
	): void {
		$.path2dRoundRect(this, x, y, w, h, radii);
	}
}
def(Path2D);
