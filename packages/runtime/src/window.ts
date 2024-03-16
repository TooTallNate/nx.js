import { assertInternalConstructor, def } from './utils';
import { initKeyboard } from './keyboard';
import {
	EventTarget,
	type EventListenerOrEventListenerObject,
} from './polyfills/event-target';
import type { console } from './console';
import type {
	Event,
	UIEvent,
	ErrorEvent,
	PromiseRejectionEvent,
} from './polyfills/event';

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
Object.setPrototypeOf(window, Window.prototype);
def(window, 'window');

export function addEventListener(
	type: 'buttondown',
	callback: EventListenerOrEventListenerObject<UIEvent>,
	options?: AddEventListenerOptions | boolean,
): void;

export function addEventListener(
	type: 'buttonup',
	callback: EventListenerOrEventListenerObject<UIEvent>,
	options?: AddEventListenerOptions | boolean,
): void;

/**
 * The `resize` event is sent to the global scope when the console changes
 * between handheld or docked mode. In handheld mode, only 720p resolution
 * is supported. In docked mode, 1080p resolution is used.
 *
 * @see https://developer.mozilla.org/docs/Web/API/Window/resize_event
 */
export function addEventListener(
	type: 'resize',
	callback: EventListenerOrEventListenerObject<Event>,
	options?: AddEventListenerOptions | boolean
): void;

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
