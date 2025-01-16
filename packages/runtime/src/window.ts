import { assertInternalConstructor, def, proto } from './utils';
import { initKeyboard } from './keyboard';
import {
	EventTarget,
	type EventListenerOrEventListenerObject,
} from './polyfills/event-target';
import type { console } from './console';
import type {
	Event,
	ErrorEvent,
	PromiseRejectionEvent,
} from './polyfills/event';
import { $ } from './$';

/**
 * The `Window` class represents the global scope within the application.
 *
 * @see https://developer.mozilla.org/docs/Web/API/Window
 */
export class Window extends EventTarget {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		super();
	}
}
def(Window);

export var window: Window & typeof globalThis = globalThis;
def(proto(window, Window), 'window');
$.windowInit(window);

Object.defineProperty(window, Symbol.toStringTag, {
	get() {
		return 'Window';
	},
});

/**
 * @see https://developer.mozilla.org/docs/Web/API/Element/keydown_event
 */
export function addEventListener(
	type: 'keydown',
	callback: EventListenerOrEventListenerObject<KeyboardEvent>,
	options?: AddEventListenerOptions | boolean,
): void;

/**
 * @see https://developer.mozilla.org/docs/Web/API/Element/keyup_event
 */
export function addEventListener(
	type: 'keyup',
	callback: EventListenerOrEventListenerObject<KeyboardEvent>,
	options?: AddEventListenerOptions | boolean,
): void;

/**
 * The `error` event is sent to the global scope when an unhandled error is thrown.
 *
 * The default behavior when this event occurs is to print the error to the screen
 * using {@link console.error | `console.error()`}, and no further application code
 * is executed. The user must then press the `+` button to exit the application.
 * Call `event.preventDefault()` to supress this default behavior.
 *
 * @see https://developer.mozilla.org/docs/Web/API/Window/error_event
 */
export function addEventListener(
	type: 'error',
	callback: EventListenerOrEventListenerObject<ErrorEvent>,
	options?: AddEventListenerOptions | boolean,
): void;

/**
 * The `unhandledrejection` event is sent to the global scope when a JavaScript
 * Promise that has no rejection handler is rejected.
 *
 * The default behavior when this event occurs is to print the error to the screen
 * using {@link console.error | `console.error()`}, and no further application code
 * is executed. The user must then press the `+` button to exit the application.
 * Call `event.preventDefault()` to supress this default behavior.
 *
 * @see https://developer.mozilla.org/docs/Web/API/Window/unhandledrejection_event
 */
export function addEventListener(
	type: 'unhandledrejection',
	callback: EventListenerOrEventListenerObject<PromiseRejectionEvent>,
	options?: AddEventListenerOptions | boolean,
): void;

/**
 * The `beforeunload` event is fired when the `+` button is pressed on the first controller.
 *
 * This event gives the application a chance to prevent the default behavior of the
 * application exiting. If the event is canceled, the application **will not exit**.
 *
 * @see https://developer.mozilla.org/docs/Web/API/Window/beforeunload_event
 */
export function addEventListener(
	type: 'beforeunload',
	callback: (event: Event) => any,
	options?: AddEventListenerOptions | boolean,
): void;

/**
 * The `unload` event is fired when the application is exiting.
 *
 * By the time this event occurs, the event loop has already been stopped,
 * so no async operations may be scheduled in the event handler.
 *
 * @see https://developer.mozilla.org/docs/Web/API/Window/unload_event
 */
export function addEventListener(
	type: 'unload',
	callback: (event: Event) => any,
	options?: AddEventListenerOptions | boolean,
): void;

export function addEventListener(
	type: string,
	callback: EventListenerOrEventListenerObject | null,
	options?: AddEventListenerOptions | boolean,
): void;

export function addEventListener(
	type: string,
	callback: EventListenerOrEventListenerObject | null,
	options?: AddEventListenerOptions | boolean,
): void {
	if (type === 'keydown' || type === 'keyup') {
		initKeyboard();
	}
	EventTarget.prototype.addEventListener.call(window, type, callback, options);
}

/**
 * Removes the event listener in target's event listener list with the same type, callback, and options.
 *
 * @see {@link https://developer.mozilla.org/docs/Web/API/EventTarget/removeEventListener | MDN Reference}
 */
export declare function removeEventListener(
	type: string,
	callback: EventListenerOrEventListenerObject | null,
	options?: EventListenerOptions | boolean,
): void;

/**
 * Dispatches a synthetic event event to target and returns true if either event's cancelable attribute value is false or its `preventDefault()` method was not invoked, and false otherwise.
 *
 * @see {@link https://developer.mozilla.org/docs/Web/API/EventTarget/dispatchEvent | MDN Reference}
 */
export declare function dispatchEvent(event: Event): boolean;

/**
 * Decodes a string of data which has been encoded using Base64 encoding.
 *
 * @see https://developer.mozilla.org/docs/Web/API/Window/atob
 */
export declare function atob(s: string): string;

/**
 * Creates a Base64-encoded ASCII string from a binary string (i.e., a string in
 * which each character in the string is treated as a byte of binary data).
 *
 * @see https://developer.mozilla.org/docs/Web/API/Window/btoa
 */
export declare function btoa(s: string): string;
