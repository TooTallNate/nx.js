import './polyfills';
import { def } from './utils';
import { SwitchClass, internal } from './switch';
import {
	setTimeout,
	setInterval,
	clearTimeout,
	clearInterval,
	processTimers,
} from './timers';
import { callRafCallbacks } from './raf';
import { console } from './console';
import {
	Event,
	ErrorEvent,
	KeyboardEvent,
	UIEvent,
	PromiseRejectionEvent,
} from './polyfills/event';

export type { SwitchClass, Versions } from './switch';
export type { Vibration } from './internal';
export type { Env } from './env';
export type { InspectOptions } from './inspect';
export type * from './types';
export type * from './console';
export type * from './navigator';
export type * from './navigator/battery';
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
export type * from './domrect';
export type { Socket, Server } from './tcp';
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

/**
 * The `WebAssembly` JavaScript object acts as the namespace for all
 * {@link https://developer.mozilla.org/docs/WebAssembly | WebAssembly}-related functionality.
 *
 * Unlike most other global objects, `WebAssembly` is not a constructor (it is not a function object).
 *
 * @see https://developer.mozilla.org/docs/WebAssembly
 */
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

import './window';
export type * from './window';

import './performance';
export type * from './performance';

import { screen } from './screen';
export type * from './screen';

export type * from './canvas/image-data';

import './canvas/path2d';
export type { Path2D } from './canvas/path2d';

import './canvas/canvas-rendering-context-2d';
export type * from './canvas/canvas-rendering-context-2d';

import './canvas/offscreen-canvas';
export type * from './canvas/offscreen-canvas';

import './canvas/offscreen-canvas-rendering-context-2d';
export type * from './canvas/offscreen-canvas-rendering-context-2d';

import { dispatchTouchEvents } from './touchscreen';

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

$.onFrame((kDown) => {
	const { keyboardInitialized, previousButtons, previousKeys } = internal;
	processTimers();
	callRafCallbacks();

	const buttonsDown = ~previousButtons & kDown;
	const buttonsUp = previousButtons & ~kDown;
	internal.previousButtons = kDown;

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
		const keys = $.hidGetKeyboardStates();
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
		internal.previousKeys = keys;
	}

	dispatchTouchEvents(screen);
});

$.onExit(() => {
	dispatchEvent(new Event('unload'));
});
