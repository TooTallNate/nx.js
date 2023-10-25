import './polyfills';
import { def } from './utils';
import { SwitchClass } from './switch';
import { INTERNAL_SYMBOL } from './types';
import {
	setTimeout,
	setInterval,
	clearTimeout,
	clearInterval,
	processTimers,
} from './timers';
import { console } from './console';
import {
	type Event,
	ErrorEvent,
	KeyboardEvent,
	TouchEvent,
	UIEvent,
	PromiseRejectionEvent,
} from './polyfills/event';
import type {
	AddEventListenerOptions,
	EventListenerOptions,
} from './polyfills/event-target';

export type { SwitchClass, Env, Vibration, Versions } from './switch';
export type { InspectOptions } from './inspect';
export type * from './types';
export type * from './console';
export type * from './navigator';
export type * from './navigator/battery';
export type {
	CanvasImageSource,
	Canvas,
	CanvasRenderingContext2D,
} from './canvas';
export type * from './canvas/image-data';
export type { Path2D } from './canvas/path2d';
export type * from './polyfills/event-target';
export type { URL, URLSearchParams } from './polyfills/url';
export type * from './polyfills/streams';
export type * from './polyfills/event';
export type * from './polyfills/blob';
export type * from './polyfills/file';
export type { TextDecoder, TextDecodeOptions } from './polyfills/text-decoder';
export type {
	TextEncoder,
	TextEncoderEncodeIntoResult,
} from './polyfills/text-encoder';
export type * from './polyfills/abort-controller';
export type * from './polyfills/streams';
export type { FontFace, FontFaceSet } from './polyfills/font';
export type * from './polyfills/form-data';
export type { BodyInit } from './fetch/body';
export type * from './fetch/headers';
export type * from './fetch/request';
export type * from './fetch/response';
export type * from './fetch/fetch';
export type * from './crypto';
export type * from './image';
export type * from './dompoint';
export type {
	TimerHandler,
	setTimeout,
	setInterval,
	clearTimeout,
	clearInterval,
} from './timers';
export type * as WebAssembly from './wasm';

/**
 * The `Switch` global object contains native interfaces to interact with the Switch hardware.
 */
const Switch = new SwitchClass();
export type { Switch };
def('Switch', Switch);
def('console', console);
def('setTimeout', setTimeout);
def('setInterval', setInterval);
def('clearTimeout', clearTimeout);
def('clearInterval', clearInterval);

import './navigator';

import * as WebAssembly from './wasm';
def('WebAssembly', WebAssembly);

import './source-map';
import { $ } from './$';

/**
 * The `import.meta` meta-property exposes context-specific metadata to a JavaScript module.
 * It contains information about the module, such as the module's URL.
 */
export interface ImportMeta {
	/**
	 * Contains the absolute URL of the JavaScript module that is being executed.
	 *
	 * @example "romfs:/main.js"
	 */
	url: string;
	/**
	 * Set to `true` when the JavaScript module that is being executed is the
	 * entrypoint file of the application.
	 */
	main: boolean;
}

function touchIsEqual(a: Touch, b: Touch) {
	return (
		a.screenX === b.screenX &&
		a.screenY === b.screenY &&
		a.radiusX === b.radiusX &&
		a.radiusY === b.radiusY &&
		a.rotationAngle === b.rotationAngle
	);
}

// Make `globalThis` inherit from `EventTarget`
Object.setPrototypeOf(globalThis, EventTarget.prototype);
EventTarget.call(globalThis);
def(
	'addEventListener',
	EventTarget.prototype.addEventListener.bind(globalThis)
);
def(
	'removeEventListener',
	EventTarget.prototype.removeEventListener.bind(globalThis)
);
def('dispatchEvent', EventTarget.prototype.dispatchEvent.bind(globalThis));

/**
 * The `error` event is sent to the global scope when an unhandled error is thrown.
 *
 * The default behavior when this event occurs is to print the error to the screen
 * using {@link console.error | `console.error()`}, and no further application code
 * is executed. The user must then press the `+` button to exit the application.
 * Call `event.preventDefault()` to supress this default behavior.
 *
 * @see https://developer.mozilla.org/en-US/docs/Web/API/Window/error_event
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
 * @see https://developer.mozilla.org/en-US/docs/Web/API/Window/unhandledrejection_event
 */
export declare function addEventListener(
	type: 'unhandledrejection',
	callback: (event: PromiseRejectionEvent) => any,
	options?: AddEventListenerOptions | boolean
): void;
export declare function addEventListener(
	type: string,
	callback: EventListenerOrEventListenerObject | null,
	options?: AddEventListenerOptions | boolean
): void;

export declare function removeEventListener(
	type: 'error',
	callback: (ev: ErrorEvent) => any,
	options?: EventListenerOptions | boolean
): void;
export declare function removeEventListener(
	type: 'unhandledrejection',
	callback: (ev: PromiseRejectionEvent) => any,
	options?: EventListenerOptions | boolean
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

$.onError((e) => {
	const ev = new ErrorEvent('error', {
		cancelable: true,
		error: e,
	});
	dispatchEvent(ev);
	if (ev.defaultPrevented) return 0;
	console.error('Uncaught', e);
	console.log('\nPress + to exit');
	return 1;
});

$.onUnhandledRejection((p, r) => {
	const ev = new PromiseRejectionEvent('unhandledrejection', {
		cancelable: true,
		promise: p,
		reason: r,
	});
	dispatchEvent(ev);
	if (ev.defaultPrevented) return 0;
	console.error('Uncaught (in promise)', r);
	console.log('\nPress + to exit');
	return 1;
});

const btnPlus = 1 << 10; ///< Plus button

Switch.addEventListener('frame', (event) => {
	const {
		keyboardInitialized,
		touchscreenInitialized,
		previousButtons,
		previousKeys,
		previousTouches,
	} = Switch[INTERNAL_SYMBOL];
	processTimers();

	const buttonsDown = ~previousButtons & event.detail;
	const buttonsUp = previousButtons & ~event.detail;
	Switch[INTERNAL_SYMBOL].previousButtons = event.detail;

	if (buttonsDown !== 0) {
		const ev = new UIEvent('buttondown', {
			cancelable: true,
			detail: buttonsDown,
		});
		Switch.dispatchEvent(ev);
		if (!ev.defaultPrevented && buttonsDown & btnPlus) {
			return Switch.exit();
		}
	}

	if (buttonsUp !== 0) {
		Switch.dispatchEvent(
			new UIEvent('buttonup', {
				detail: buttonsUp,
			})
		);
	}

	if (keyboardInitialized) {
		const keys = Switch.native.hidGetKeyboardStates();
		for (let i = 0; i < 4; i++) {
			const keysDown = ~previousKeys[i] & keys[i];
			const keysUp = previousKeys[i] & ~keys[i];
			for (let k = 0; k < 64; k++) {
				if (keysDown & (1n << (BigInt(k) & 63n))) {
					const o = {
						keyCode: i * 64 + k,
						modifiers: keys.modifiers,
					};
					Switch.dispatchEvent(new KeyboardEvent('keydown', o));
				}
				if (keysUp & (1n << (BigInt(k) & 63n))) {
					const o = {
						keyCode: i * 64 + k,
						modifiers: keys.modifiers,
					};
					Switch.dispatchEvent(new KeyboardEvent('keyup', o));
				}
			}
		}
		Switch[INTERNAL_SYMBOL].previousKeys = keys;
	}

	if (touchscreenInitialized) {
		const touches = Switch.native.hidGetTouchScreenStates();
		if (touches) {
			const startTouches: Touch[] = [];
			const changedTouches: Touch[] = [];
			const endTouches: Touch[] = [];
			const touchIds = new Set<number>();

			for (const touch of touches) {
				let started = true;
				for (const prevTouch of previousTouches) {
					if (touch.identifier === (prevTouch.identifier | 0)) {
						started = false;
						// @ts-expect-error
						touch.identifier = prevTouch.identifier;
						touchIds.add(touch.identifier);
						if (!touchIsEqual(touch, prevTouch)) {
							changedTouches.push(touch);
						}
						break;
					}
				}
				if (started) {
					// @ts-expect-error
					touch.identifier += Math.random();
					touchIds.add(touch.identifier);
					startTouches.push(touch);
				}
			}

			Switch[INTERNAL_SYMBOL].previousTouches = touches;

			for (const prevTouch of previousTouches) {
				if (!touchIds.has(prevTouch.identifier)) {
					endTouches.push(prevTouch);
				}
			}

			if (startTouches.length) {
				Switch.dispatchEvent(
					new TouchEvent('touchstart', {
						touches,
						changedTouches: startTouches,
					})
				);
			}
			if (changedTouches.length) {
				Switch.dispatchEvent(
					new TouchEvent('touchmove', {
						touches,
						changedTouches,
					})
				);
			}
			if (endTouches.length) {
				Switch.dispatchEvent(
					new TouchEvent('touchend', {
						touches,
						changedTouches: endTouches,
					})
				);
			}
		} else if (previousTouches.length) {
			// No current touches, but there were previous touches, so fire "touchend"
			Switch.dispatchEvent(
				new TouchEvent('touchend', {
					touches: [],
					changedTouches: previousTouches,
				})
			);
			Switch[INTERNAL_SYMBOL].previousTouches = [];
		}
	}
});

Switch.addEventListener('exit', () => {
	Switch[INTERNAL_SYMBOL].cleanup();
});
