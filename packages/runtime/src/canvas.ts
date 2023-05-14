import { INTERNAL_SYMBOL } from './switch';

export class Canvas {
	width: number;
	height: number;

	constructor(width: number, height: number) {
		this.width = width;
		this.height = height;
	}

	getContext(contextId: '2d'): CanvasRenderingContext2D {
		if (contextId !== '2d') {
			throw new TypeError(
				`"${contextId}" is not supported. Must be "2d".`
			);
		}
		return new CanvasRenderingContext2D(this);
	}
}

export class CanvasRenderingContext2D {
	readonly canvas: Canvas;
	[INTERNAL_SYMBOL]: Uint8ClampedArray;

	constructor(canvas: Canvas) {
		this.canvas = canvas;
		this[INTERNAL_SYMBOL] = new Uint8ClampedArray(
			canvas.width * canvas.height * 4
		);
	}

	getImageData(): Uint8ClampedArray {
		return this[INTERNAL_SYMBOL];
	}
}
