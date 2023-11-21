import { assertInternalConstructor, def } from './utils';
import { EventTarget } from './polyfills/event-target';
import { INTERNAL_SYMBOL } from './internal';

export class Screen extends EventTarget implements globalThis.Screen {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		super();
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
		throw new Error('Not implemented');
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Screen/pixelDepth) */
	get pixelDepth() {
		return 24;
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Screen/width) */
	get width() {
		return 1280;
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/Screen/height) */
	get height() {
		return 720;
	}
}
def('Screen', Screen);

// @ts-expect-error Internal constructor
export const screen = new Screen(INTERNAL_SYMBOL);
def('screen', screen);
