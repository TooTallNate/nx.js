import { $ } from '../$';
import { INTERNAL_SYMBOL } from '../internal';
import type { Blob } from '../polyfills/blob';
import { EventTarget } from '../polyfills/event-target';
import type { ImageEncodeOptions } from '../types';
import { createInternal, def, proto } from '../utils';
import { OffscreenCanvasRenderingContext2D } from './offscreen-canvas-rendering-context-2d';

interface OffscreenCanvasInternal {
	context2d?: OffscreenCanvasRenderingContext2D;
}

const _ = createInternal<OffscreenCanvas, OffscreenCanvasInternal>();

/**
 * @see https://developer.mozilla.org/docs/Web/API/OffscreenCanvas
 */
export class OffscreenCanvas
	extends EventTarget
	implements Omit<globalThis.OffscreenCanvas, 'getContext'>
{
	/**
	 * Specifies the width (in pixels) of the canvas.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/OffscreenCanvas/height
	 */
	declare width: number;

	/**
	 * Specifies the height (in pixels) of the canvas.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/OffscreenCanvas/height
	 */
	declare height: number;

	/**
	 * @param width The width of the offscreen canvas.
	 * @param height The height of the offscreen canvas.
	 */
	constructor(width: number, height: number) {
		super();
		const c = proto($.canvasNew(width, height), OffscreenCanvas);
		_.set(c, {});
		return c;
	}

	convertToBlob(options?: ImageEncodeOptions | undefined): Promise<Blob> {
		throw new Error('Method not implemented.');
	}

	getContext(
		contextId: '2d',
		options?: any,
	): OffscreenCanvasRenderingContext2D {
		if (contextId !== '2d') {
			throw new TypeError(
				`OffscreenCanvas.getContext: '${contextId}' (value of argument 1) is not a valid value for enumeration OffscreenRenderingContextId.`,
			);
		}
		const i = _(this);
		if (!i.context2d) {
			i.context2d = new OffscreenCanvasRenderingContext2D(
				// @ts-expect-error
				INTERNAL_SYMBOL,
				this,
			);
		}
		return i.context2d;
	}

	transferToImageBitmap(): ImageBitmap {
		throw new Error('Method not implemented.');
	}

	//oncontextlost: ((this: globalThis.OffscreenCanvas, ev: Event) => any) | null;
	//oncontextrestored: ((this: globalThis.OffscreenCanvas, ev: Event) => any) | null;
	oncontextlost = null;
	oncontextrestored = null;

	//addEventListener<K extends keyof OffscreenCanvasEventMap>(type: K, listener: (this: globalThis.OffscreenCanvas, ev: OffscreenCanvasEventMap[K]) => any, options?: boolean | AddEventListenerOptions): void;
	//addEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | AddEventListenerOptions): void;
	//addEventListener(type: unknown, listener: unknown, options?: unknown): void {
	//    throw new Error("Method not implemented.");
	//}
	//removeEventListener<K extends keyof OffscreenCanvasEventMap>(type: K, listener: (this: globalThis.OffscreenCanvas, ev: OffscreenCanvasEventMap[K]) => any, options?: boolean | EventListenerOptions): void;
	//removeEventListener(type: string, listener: EventListenerOrEventListenerObject, options?: boolean | EventListenerOptions): void;
	//removeEventListener(type: unknown, listener: unknown, options?: unknown): void {
	//    throw new Error("Method not implemented.");
	//}
	//dispatchEvent(event: Event): boolean {
	//    throw new Error("Method not implemented.");
	//}
}
$.canvasInitClass(OffscreenCanvas);
def(OffscreenCanvas);
