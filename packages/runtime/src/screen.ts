import { $ } from './$';
import { assertInternalConstructor, createInternal, def } from './utils';
import { EventTarget } from './polyfills/event-target';
import { INTERNAL_SYMBOL } from './internal';
import { CanvasRenderingContext2D } from './canvas/canvas-rendering-context-2d';

interface ScreenInternal {
	context2d?: CanvasRenderingContext2D;
}

const _ = createInternal<Screen, ScreenInternal>();

export class Screen extends EventTarget implements globalThis.Screen {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		super();
		const c = $.canvasNew(1280, 720) as Screen;
		Object.setPrototypeOf(c, Screen.prototype);
		_.set(c, {});
		return c;
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Screen/availWidth) */
	get availWidth() {
		return this.width;
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Screen/availHeight) */
	get availHeight() {
		return this.height;
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Screen/colorDepth) */
	get colorDepth() {
		return 24;
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Screen/orientation) */
	get orientation(): ScreenOrientation {
		throw new Error('Method not implemented.');
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Screen/pixelDepth) */
	get pixelDepth() {
		return 24;
	}

	/**
	 * The width of the screen in CSS pixels.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Screen/width
	 */
	declare readonly width: number;

	/**
	 * The height of the screen in CSS pixels.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Screen/height
	 */
	declare readonly height: number;

	/**
	 * Creates a {@link Blob} object representing the image contained on the screen.
	 *
	 * @example
	 *
	 * ```typescript
	 * screen.toBlob(blob => {
	 *   blob.arrayBuffer().then(buffer => {
	 *     Switch.writeFileSync('out.png', buffer);
	 *   });
	 * });
	 * ```
	 *
	 * @param callback A callback function with the resulting {@link Blob} object as a single argument. `null` may be passed if the image cannot be created for any reason.
	 * @param type A string indicating the image format. The default type is `image/png`. This image format will be also used if the specified type is not supported.
	 * @param quality A number between `0` and `1` indicating the image quality to be used when creating images using file formats that support lossy compression (such as `image/jpeg`). A user agent will use its default quality value if this option is not specified, or if the number is outside the allowed range.
	 */
	toBlob(
		callback: (blob: Blob | null) => void,
		type = 'image/png',
		quality = 0.8
	) {
		throw new Error('Method not implemented.');
	}

	/**
	 * Returns a `data:` URL containing a representation of the image in the format specified by the type parameter.
	 *
	 * @example
	 *
	 * ```typescript
	 * const url = screen.toDataURL();
	 * fetch(url)
	 *   .then(res => res.arrayBuffer())
	 *   .then(buffer => {
	 *     Switch.writeFileSync('out.png', buffer);
	 *   });
	 * ```
	 *
	 * @param type A string indicating the image format. The default type is `image/png`. This image format will be also used if the specified type is not supported.
	 * @param quality A number between `0` and `1` indicating the image quality to be used when creating images using file formats that support lossy compression (such as `image/jpeg`). The default quality value will be used if this option is not specified, or if the number is outside the allowed range.
	 * @see https://developer.mozilla.org/docs/Web/API/HTMLCanvasElement/toDataURL
	 */
	toDataURL(type = 'image/png', quality = 0.8) {
		throw new Error('Method not implemented.');
	}

	// Compat with HTML DOM interface
	className = '';
	get nodeType() {
		return 1;
	}
	get nodeName() {
		return 'CANVAS';
	}
	getAttribute(name: string): string | null {
		if (name === 'width') return String(this.width);
		if (name === 'height') return String(this.height);
		return null;
	}
	setAttribute(name: string, value: string | number) {}

	getContext(contextId: '2d'): CanvasRenderingContext2D {
		if (contextId !== '2d') {
			throw new TypeError('Only "2d" rendering context is supported');
		}

		const i = _(this);
		if (!i.context2d) {
			i.context2d = new CanvasRenderingContext2D(
				// @ts-expect-error Internal constructor
				INTERNAL_SYMBOL,
				this
			);

			$.framebufferInit(this);
		}

		return i.context2d;
	}
}
$.canvasInitClass(Screen);
def('Screen', Screen);

// @ts-expect-error Internal constructor
export const screen = new Screen(INTERNAL_SYMBOL);
def('screen', screen);
