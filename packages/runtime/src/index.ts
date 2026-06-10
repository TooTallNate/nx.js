import './polyfills';
import { Console, console } from './console';
import { presentConsole } from './console-screen';
import { ErrorEvent, Event, PromiseRejectionEvent } from './polyfills/event';
import { callRafCallbacks } from './raf';
import {
	clearInterval,
	clearTimeout,
	processTimers,
	setInterval,
	setTimeout,
} from './timers';
import { def } from './utils';

export type * from './console';
export type * from './terminal';

export type * from './crypto';
import { DOMException } from './dom-exception';
export { DOMException } from './dom-exception';
def(DOMException);
export type * from './dommatrix';
export type { DOMPoint, DOMPointInit, DOMPointReadOnly } from './dompoint';
export type * from './domrect';
export type { BodyInit } from './fetch/body';
export type * from './fetch/fetch';
export type * from './fetch/headers';
export type * from './fetch/request';
export type * from './fetch/response';
export type { FontFace } from './font/font-face';
export type { FontFaceSet, fonts } from './font/font-face-set';
export type * from './image';
export type * from './navigator';
export type * from './navigator/battery';
export type * from './navigator/bluetooth';
export type {
	Gamepad,
	GamepadButton,
	GamepadHapticActuator,
} from './navigator/gamepad';
export type { VirtualKeyboard } from './navigator/virtual-keyboard';
export type * from './polyfills/abort-controller';
export type * from './polyfills/blob';
export type * from './polyfills/event';
export type * from './polyfills/event-target';
export type * from './polyfills/file';
export type * from './polyfills/form-data';
export type * from './polyfills/streams';
export type { TextDecodeOptions, TextDecoder } from './polyfills/text-decoder';
export type {
	TextEncoder,
	TextEncoderEncodeIntoResult,
} from './polyfills/text-encoder';
export type { URL, URLSearchParams } from './polyfills/url';
export type {
	cancelAnimationFrame,
	FrameRequestCallback,
	requestAnimationFrame,
} from './raf';
export type * from './sensor';
export type {
	clearInterval,
	clearTimeout,
	setInterval,
	setTimeout,
	TimerHandler,
} from './timers';
export type * from './types';
export type * from './uint8array';

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
// WebAssembly is provided NATIVELY by V8 (the wasm3-backed `./wasm` shim was
// removed in the V8 migration). Do not override the native global.

/**
 * The `Switch` global object contains native interfaces to interact with the Switch hardware.
 */
export type * as Switch from './switch';

import * as Switch from './switch';

def(Switch, 'Switch');

def(console, 'console');
def(Console);
def(setTimeout, 'setTimeout');
def(setInterval, 'setInterval');
def(clearTimeout, 'clearTimeout');
def(clearInterval, 'clearInterval');
def($.queueMicrotask, 'queueMicrotask');

import './navigator';

import './source-map';
import { $ } from './$';

// WebAssembly availability guard.
//
// WASM requires V8's JIT (a code-generation backend) AND room in the JIT code
// arena for its separate code space. Two configurations make it unavailable:
//   - jitless mode (`[v8] jit = off`, or — without this guard — applet mode if
//     JIT were disabled): V8 has no backend, so `new WebAssembly.Module()`
//     throws an opaque CompileError.
//   - applet mode with no WASM code headroom (the regime default,
//     `$.config.codeHeadroomMb === 0`): there is no room reserved for WASM's
//     code space, so compilation OOMs.
// In both cases the native error is cryptic. Wrap the `WebAssembly` entry points
// to fail fast with an actionable message pointing at the `nxjs.ini` fix.
{
	const WA: any = (globalThis as any).WebAssembly;
	if (WA) {
		const jit = $.config?.jit !== false;
		const headroom = $.config?.codeHeadroomMb ?? 0;
		let reason: string | undefined;
		if (!jit) {
			reason =
				'WebAssembly requires the V8 JIT, which is disabled ' +
				'(`[v8] jit = off` in nxjs.ini). Remove that override (or set ' +
				'`jit = on`) to use WebAssembly.';
		} else if (headroom === 0) {
			reason =
				'WebAssembly is disabled in this memory regime (applet mode) ' +
				'because no JIT code-space headroom is reserved by default. ' +
				'Enable it by adding to nxjs.ini:\n\n  [v8]\n  wasm = on\n\n' +
				'(equivalently `code_headroom_mb = 64`). Note this needs the ' +
				'extra memory, so prefer launching in full-memory/application ' +
				'mode for WebAssembly-heavy apps.';
		}
		if (reason) {
			const err = () => {
				throw new WA.CompileError(`[nx.js] ${reason}`);
			};
			// Synchronous entry points throw; async ones reject.
			const rej = () => Promise.reject(new WA.CompileError(`[nx.js] ${reason}`));
			WA.Module = function () {
				err();
			} as any;
			WA.compile = rej;
			WA.instantiate = rej;
			WA.compileStreaming = rej;
			WA.instantiateStreaming = rej;
		}
	}
}

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

import './audio/audio-buffer';
export type * from './audio/audio-buffer';

import './audio/audio-param';
export type * from './audio/audio-param';

import './audio/audio-node';
export type * from './audio/audio-node';

import './audio/audio-scheduled-source-node';
export type * from './audio/audio-scheduled-source-node';

import './audio/audio-buffer-source-node';
export type * from './audio/audio-buffer-source-node';

import './audio/gain-node';
export type * from './audio/gain-node';

import './audio/stereo-panner-node';
export type * from './audio/stereo-panner-node';

import './audio/audio-destination-node';
export type * from './audio/audio-destination-node';

import './audio/base-audio-context';
export type * from './audio/base-audio-context';

import './audio/audio-context';
export type * from './audio/audio-context';

import './audio/offline-audio-completion-event';
export type * from './audio/offline-audio-completion-event';

import './audio/offline-audio-context';
export type * from './audio/offline-audio-context';

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

import './compression-streams';

export type * from './compression-streams';

import './websocket';

export type {
	BinaryType,
	CloseEvent,
	CloseEventInit,
	MessageEvent,
	MessageEventInit,
	WebSocket,
} from './websocket';

import { dispatchKeyboardEvents } from './keyboard';
import { dispatchTouchEvents } from './touchscreen';
import { sweepGamepadConnections } from './navigator/gamepad';

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
	// Emit `gamepadconnected` / `gamepaddisconnected` on controller hotplug.
	for (const e of sweepGamepadConnections()) {
		globalThis.dispatchEvent(e);
	}
	// Blit the default console's terminal canvas onto the screen if it changed
	// (no-op once the app owns screen rendering). After rAF so an app that
	// draws to the screen each frame isn't overwritten on the same frame.
	presentConsole();
});

$.onExit(() => {
	dispatchEvent(new Event('unload'));
});
