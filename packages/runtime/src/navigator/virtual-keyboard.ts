import { $ } from '../$';
import { DOMRect } from '../domrect';
import { assertInternalConstructor, def } from '../utils';
import { Event } from '../polyfills/event';
import { EventTarget } from '../polyfills/event-target';
import { requestAnimationFrame, cancelAnimationFrame } from '../raf';

let update: () => void;
let id = -1;
let value = '';
let cursorPos = 0;

/**
 * @see https://developer.mozilla.org/docs/Web/API/VirtualKeyboard
 */
export class VirtualKeyboard extends EventTarget {
	/**
	 * Indicates the position and size of the on-screen virtual keyboard that overlays the screen.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/VirtualKeyboard/boundingRect
	 */
	readonly boundingRect!: DOMRect;

	/**
	 * Set the type of virtual keyboard.
	 */
	type?: number;

	/**
	 * Text to display for the "OK" button. Max of 8 characters.
	 *
	 * @example "Submit"
	 */
	okButtonText?: string;

	/**
	 * Single character to use for the left-side button.
	 *
	 * @example "-"
	 * @note Only for "NumPad" keyboard type.
	 */
	leftButtonText?: string;

	/**
	 * Single character to use for the right-side button.
	 *
	 * @example "+"
	 * @note Only for "NumPad" keyboard type.
	 */
	rightButtonText?: string;

	/**
	 * If set to `true`, then the dictionary will be enabled
	 * for faster user input based on predictive text.
	 */
	enableDictionary?: boolean;

	/**
	 * If set to `true`, then the "Return" key will be enabled,
	 * allowing for newlines to be included in the input.
	 */
	enableReturn?: boolean;

	/**
	 * Specifies the min string length. When the input
	 * is too short, the "OK" button will be disabled.
	 */
	minLength?: number;

	/**
	 * Specifies the max string length. When the input
	 * is too long, input will stop being accepted.
	 */
	maxLength?: number;

	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		super();
	}

	get overlaysContent() {
		return true;
	}

	get value() {
		return value;
	}

	get cursorIndex() {
		return cursorPos;
	}

	show() {
		Object.assign(this.boundingRect, $.swkbdShow(this));
		this.dispatchEvent(new Event('geometrychange'));
		id = requestAnimationFrame(update);
	}

	hide() {
		$.swkbdHide(this);
		onHide(this);
	}
}
def('VirtualKeyboard', VirtualKeyboard);

function onHide(k: VirtualKeyboard) {
	cancelAnimationFrame(id);
	const b = k.boundingRect;
	b.x = b.y = b.width = b.height = 0;
	k.dispatchEvent(new Event('geometrychange'));
}

export function create() {
	const k = $.swkbdCreate({
		onCancel() {
			onHide(k);
		},
		onChange(str, ci) {
			value = str;
			cursorPos = ci;
			k.dispatchEvent(new Event('change'));
		},
		onCursorMove(str, ci) {
			value = str;
			cursorPos = ci;
			k.dispatchEvent(new Event('cursormove'));
		},
		onSubmit(str) {
			value = str;
			onHide(k);
			k.dispatchEvent(new Event('submit'));
		},
	});
	Object.setPrototypeOf(k, VirtualKeyboard.prototype);
	// @ts-expect-error
	k.boundingRect = new DOMRect();
	update = () => {
		$.swkbdUpdate.call(k);
		id = requestAnimationFrame(update);
	};
	addEventListener('unload', $.swkbdExit.bind(k));
	return k;
}
