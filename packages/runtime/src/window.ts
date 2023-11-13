import type { console } from './console';
import type {
	Event,
	ErrorEvent,
	PromiseRejectionEvent,
} from './polyfills/event';
import { assertInternalConstructor, def } from './utils';

export class Window extends EventTarget {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		super();
	}
}
def('Window', Window);

export const window: Window & typeof globalThis = globalThis;
def('window', window);
Object.setPrototypeOf(window, Window.prototype);
EventTarget.call(window);

// Temporary band-aid since EventTarget does not default `this` to `globalThis`
def('addEventListener', window.addEventListener.bind(window));
def('removeEventListener', window.removeEventListener.bind(window));
def('dispatchEvent', window.dispatchEvent.bind(window));

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
export declare function addEventListener(
	type: 'error',
	callback: (event: ErrorEvent) => any,
	options?: AddEventListenerOptions | boolean
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
export declare function addEventListener(
	type: 'unhandledrejection',
	callback: (event: PromiseRejectionEvent) => any,
	options?: AddEventListenerOptions | boolean
): void;

/**
 * The `unload` event is fired when the application is exiting.
 *
 * By the time this event occurs, the event loop has already been stopped,
 * so no async operations may be scheduled in the event handler.
 *
 * @see https://developer.mozilla.org/docs/Web/API/Window/unload_event
 */
export declare function addEventListener(
	type: 'unload',
	callback: (event: Event) => any,
	options?: AddEventListenerOptions | boolean
): void;

export declare function addEventListener(
	type: string,
	callback: EventListenerOrEventListenerObject | null,
	options?: AddEventListenerOptions | boolean
): void;

/**
 * Removes the event listener in target's event listener list with the same type, callback, and options.
 *
 * @see {@link https://developer.mozilla.org/docs/Web/API/EventTarget/removeEventListener | MDN Reference}
 */
export declare function removeEventListener(
	type: string,
	callback: EventListenerOrEventListenerObject | null,
	options?: EventListenerOptions | boolean
): void;

/**
 * Dispatches a synthetic event event to target and returns true if either event's cancelable attribute value is false or its `preventDefault()` method was not invoked, and false otherwise.
 *
 * @see {@link https://developer.mozilla.org/docs/Web/API/EventTarget/dispatchEvent | MDN Reference}
 */
export declare function dispatchEvent(event: Event): boolean;
