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
import {
	Event,
	ErrorEvent,
	UIEvent,
	PromiseRejectionEvent,
} from './polyfills/event';

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

import './canvas/image-bitmap';
export type * from './canvas/image-bitmap';

export type * from './canvas/image-data';

import './canvas/path2d';
export type { Path2D } from './canvas/path2d';

import './canvas/canvas-rendering-context-2d';
export type * from './canvas/canvas-rendering-context-2d';

import './canvas/offscreen-canvas';
export type * from './canvas/offscreen-canvas';

import './canvas/offscreen-canvas-rendering-context-2d';
export type * from './canvas/offscreen-canvas-rendering-context-2d';

import './audio/audio-context';
export type * from './audio/base-audio-context';
export type * from './audio/audio-context';

import './ambientlightsensor';
export type * from './ambientlightsensor';

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

const btnPlus = 1 << 10; ///< Plus button
let previousButtons = 0;

$.onFrame((kDown) => {
	processTimers();
	callRafCallbacks();

	const buttonsDown = ~previousButtons & kDown;
	const buttonsUp = previousButtons & ~kDown;
	previousButtons = kDown;

	if (buttonsDown !== 0) {
		const ev = new UIEvent('buttondown', {
			cancelable: true,
			detail: buttonsDown,
		});
		globalThis.dispatchEvent(ev);
		if (!ev.defaultPrevented && buttonsDown & btnPlus) {
			return $.exit();
		}
	}

	if (buttonsUp !== 0) {
		globalThis.dispatchEvent(
			new UIEvent('buttonup', {
				detail: buttonsUp,
			})
		);
	}

	dispatchKeyboardEvents(globalThis);
	dispatchTouchEvents(screen);
});

$.onExit(() => {
	dispatchEvent(new Event('unload'));
});
