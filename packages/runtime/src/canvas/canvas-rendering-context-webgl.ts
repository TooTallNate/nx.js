import { $ } from '../$';
import {
	createInternal,
	assertInternalConstructor,
	def,
	stub,
} from '../utils';
import type { Screen } from '../screen';

interface CanvasRenderingContextWebGLInternal {
	canvas: Screen;
}

const _ = createInternal<
	CanvasRenderingContextWebGL,
	CanvasRenderingContextWebGLInternal
>();

export class CanvasRenderingContextWebGL {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		const canvas: Screen = arguments[1];
		const ctx = $.canvasContextWebGLNew(canvas);
		Object.setPrototypeOf(ctx, CanvasRenderingContextWebGL.prototype);
		_.set(ctx, { canvas });
		return ctx;
	}

	clearColor(red: number, green: number, blue: number, alpha: number): void {
		stub();
	}

	clear(mask: number): void {
		stub();
	}
}
$.canvasContextWebGLInitClass(CanvasRenderingContextWebGL);
def(CanvasRenderingContextWebGL);
