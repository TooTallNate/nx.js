import { assertInternalConstructor, def } from '../utils';
import type { DOMMatrix2DInit } from '../dommatrix';
import type { CanvasRenderingContext2D } from './canvas-rendering-context-2d';

/**
 * Represents an opaque object describing a pattern, based on an image, or canvas.
 *
 * It can be used as a {@link CanvasRenderingContext2D.fillStyle | `fillStyle`} or {@link CanvasRenderingContext2D.strokeStyle| `strokeStyle`}.
 *
 * Use the {@link CanvasRenderingContext2D.createPattern | `ctx.createPattern()`} method to create an instance.
 *
 * @see https://developer.mozilla.org/docs/Web/API/CanvasPattern
 */
export class CanvasPattern implements globalThis.CanvasPattern {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
	}

	/**
	 * Uses a `DOMMatrix` object as the pattern's transformation matrix and invokes it on the pattern.
	 *
	 * @param transform A `DOMMatrix` to use as the pattern's transformation matrix.
	 * @see https://developer.mozilla.org/docs/Web/API/CanvasPattern/setTransform
	 */
	setTransform(transform?: DOMMatrix2DInit): void {
		throw new Error('Method not implemented.');
	}
}
def(CanvasPattern);
