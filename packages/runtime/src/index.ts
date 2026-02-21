import './polyfills';
import { def } from './utils';
import {
	setTimeout,
	setInterval,
	clearTimeout,
	clearInterval,
	processTimers,
} from './timers';
import { callRafCallbacks } from './raf';
import { console } from './console';
import { Event, ErrorEvent, PromiseRejectionEvent } from './polyfills/event';

export type * from './types';
export type * from './console';
export type * from './navigator';
export type * from './navigator/battery';
export type {
	Gamepad,
	GamepadButton,
	GamepadHapticActuator,
} from './navigator/gamepad';
export type { VirtualKeyboard } from './navigator/virtual-keyboard';
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
export type { FontFaceSet, fonts } from './font/font-face-set';
export type { FontFace } from './font/font-face';
export type * from './polyfills/form-data';
export type { BodyInit } from './fetch/body';
export type * from './fetch/headers';
export type * from './fetch/request';
export type * from './fetch/response';
export type * from './fetch/fetch';
export type * from './crypto';
export type * from './image';
export type { DOMPoint, DOMPointInit, DOMPointReadOnly } from './dompoint';
export type * from './dommatrix';
export type * from './domrect';
export type * from './sensor';
export type {
	TimerHandler,
	setTimeout,
	setInterval,
	clearTimeout,
	clearInterval,
} from './timers';
export type {
	FrameRequestCallback,
	requestAnimationFrame,
	cancelAnimationFrame,
} from './raf';

import './storage';
export type * from './storage';

/**
 * The `WebAssembly` JavaScript object acts as the namespace for all
 * {@link https://developer.mozilla.org/docs/WebAssembly | WebAssembly}-related functionality.
 *
 * Unlike most other global objects, `WebAssembly` is not a constructor (it is not a function object).
 *
 * @see https://developer.mozilla.org/docs/WebAssembly
 */
export type * as WebAssembly from './wasm';
import * as WebAssembly from './wasm';
def(WebAssembly, 'WebAssembly');

/**
 * The `Switch` global object contains native interfaces to interact with the Switch hardware.
 */
export type * as Switch from './switch';
import * as Switch from './switch';
def(Switch, 'Switch');

def(console, 'console');
def(setTimeout);
def(setInterval);
def(clearTimeout);
def(clearInterval);

import './navigator';

import './source-map';
import { $ } from './$';

/**
 * The `import.meta` meta-property exposes context-specific metadata to a JavaScript module.
 * It contains information about the module, such as the module's URL.
 *
 * @see https://developer.mozilla.org/docs/Web/JavaScript/Reference/Operators/import.meta
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

/**
 * Queues a microtask to be executed at a safe time prior
 * to control returning to the runtime's event loop.
 *
 * @param callback A function to be executed when the runtime determines it is safe to invoke.
 * @see https://developer.mozilla.org/docs/Web/API/queueMicrotask
 */
export declare function queueMicrotask(callback: () => void): void;

import './audio';
import './web-applet';
export type * from './audio';

import './window';
export type * from './window';

import './performance';
export type * from './performance';

import { screen } from './screen';
export type * from './screen';

import './canvas/image-bitmap';
export type * from './canvas/image-bitmap';

export type * from './canvas/image-data';

import './canvas/path2d';
export type { Path2D } from './canvas/path2d';

import './canvas/canvas-gradient';
export type * from './canvas/canvas-gradient';

import './canvas/canvas-rendering-context-2d';
export type * from './canvas/canvas-rendering-context-2d';

import './canvas/offscreen-canvas';
export type * from './canvas/offscreen-canvas';

import './canvas/offscreen-canvas-rendering-context-2d';
export type * from './canvas/offscreen-canvas-rendering-context-2d';

import './ambientlightsensor';
export type * from './ambientlightsensor';

import './dom-exception';
export type * from './dom-exception';

import './compression-streams';
export type * from './compression-streams';

import { dispatchTouchEvents } from './touchscreen';
import { dispatchKeyboardEvents } from './keyboard';

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

$.onFrame((plusDown) => {
	// Default behavior is to exit when the "+" button is pressed on the first controller.
	// Can be prevented by calling `preventDefault()` on the "beforeunload" event.
	if (plusDown) {
		const e = new Event('beforeunload', { cancelable: true });
		globalThis.dispatchEvent(e);
		if (!e.defaultPrevented) {
			return $.exit();
		}
	}

	processTimers();
	callRafCallbacks();
	dispatchTouchEvents(screen);
	dispatchKeyboardEvents(globalThis);
});

$.onExit(() => {
	dispatchEvent(new Event('unload'));
});
