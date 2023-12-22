import { $ } from './$';
import { assertInternalConstructor, def } from './utils';
import { BatteryManager } from './navigator/battery';
import { INTERNAL_SYMBOL, VibrationValues } from './internal';
import { setTimeout, clearTimeout } from './timers';
import {
	type VirtualKeyboard,
	create as newVirtualKeyboard,
} from './navigator/virtual-keyboard';
import type { Vibration } from './switch';

interface NavigatorState {
	batt?: BatteryManager;
	vk?: VirtualKeyboard;

	vibrationDevicesInitialized?: boolean;
	vibrationPattern?: (number | Vibration)[];
	vibrationTimeoutId?: number;
}

const DEFAULT_VIBRATION: VibrationValues = {
	lowAmp: 0.2,
	lowFreq: 160,
	highAmp: 0.2,
	highFreq: 320,
};

const STOP_VIBRATION: VibrationValues = {
	lowAmp: 0,
	lowFreq: 160,
	highAmp: 0,
	highFreq: 320,
};

const state: NavigatorState = {};

/**
 * The `Navigator` interface represents the state and the identity of the user agent.
 * It allows scripts to query it and to register themselves to carry on some activities.
 *
 * A `Navigator` instance can be retrieved by accessing the global {@link navigator | `navigator`} property.
 *
 * @see https://developer.mozilla.org/docs/Web/API/Navigator
 */
export class Navigator {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
	}

	/**
	 * Identifies the platform on which the application is running.
	 *
	 * @example "Horizon arm64"
	 * @see https://developer.mozilla.org/docs/Web/API/Navigator/platform
	 */
	get platform() {
		return 'Horizon arm64';
	}

	/**
	 * The value used for the `User-Agent` request header for
	 * HTTP requests initiated with {@link fetch | `fetch()`}.
	 *
	 * @example "my-app/0.0.1 (Switch; en-us; rv:14.1.2|AMS 1.5.4|E) nx.js/0.0.18"
	 * @see https://developer.mozilla.org/docs/Web/API/Navigator/userAgent
	 */
	get userAgent() {
		return `nx.js/${$.version.nxjs}`;
	}

	/**
	 * Returns the maximum number of simultaneous touch contact points are
	 * supported by the current device.
	 *
	 * @example 16
	 * @see https://developer.mozilla.org/docs/Web/API/Navigator/maxTouchPoints
	 */
	get maxTouchPoints() {
		// Value of `struct HidTouchScreenState` -> `HidTouchState touches[16]`
		return 16;
	}

	/**
	 * Returns a promise which is resolved to a {@link BatteryManager} instance.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Navigator/getBattery
	 */
	async getBattery() {
		let b = state.batt;
		if (!b) {
			// @ts-expect-error Internal constructor
			b = state.batt = new BatteryManager(INTERNAL_SYMBOL);
		}
		return b;
	}

	/**
	 * Vibrates the main gamepad for the specified number of milliseconds or pattern.
	 *
	 * If a vibration pattern is already in progress when this method is called,
	 * the previous pattern is halted and the new one begins instead.
	 *
	 * @example
	 *
	 * ```typescript
	 * // Vibrate for 200ms with the default amplitude/frequency values
	 * navigator.vibrate(200);
	 *
	 * // Vibrate 'SOS' in Morse Code
	 * navigator.vibrate([
	 *   100, 30, 100, 30, 100, 30, 200, 30, 200, 30, 200, 30, 100, 30, 100, 30, 100,
	 * ]);
	 *
	 * // Specify amplitude/frequency values for the vibration
	 * navigator.vibrate({
	 *   duration: 500,
	 *   lowAmp: 0.2
	 *   lowFreq: 160,
	 *   highAmp: 0.6,
	 *   highFreq: 500
	 * });
	 * ```
	 *
	 * @param pattern Provides a pattern of vibration and pause intervals. Each value indicates a number of milliseconds to vibrate or pause, in alternation. You may provide either a single value (to vibrate once for that many milliseconds) or an array of values to alternately vibrate, pause, then vibrate again.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Navigator/vibrate
	 */
	vibrate(pattern: number | Vibration | (number | Vibration)[]): boolean {
		if (!Array.isArray(pattern)) {
			pattern = [pattern];
		}
		const patternValues: (number | Vibration)[] = [];
		for (let i = 0; i < pattern.length; i++) {
			let p = pattern[i];
			if (i % 2 === 0) {
				// Even - vibration interval
				if (typeof p === 'number') {
					p = {
						...DEFAULT_VIBRATION,
						duration: p,
					};
				}
				if (
					p.highAmp < 0 ||
					p.highAmp > 1 ||
					p.lowAmp < 0 ||
					p.lowAmp > 1
				) {
					return false;
				}
				patternValues.push(p);
			} else {
				// Odd - pause interval
				if (typeof p !== 'number') return false;
				patternValues.push(p);
			}
		}
		if (!state.vibrationDevicesInitialized) {
			$.hidInitializeVibrationDevices();
			$.hidSendVibrationValues(DEFAULT_VIBRATION);
			state.vibrationDevicesInitialized = true;
		}
		if (typeof state.vibrationTimeoutId === 'number') {
			clearTimeout(state.vibrationTimeoutId);
		}
		state.vibrationPattern = patternValues;
		state.vibrationTimeoutId = setTimeout(processVibrations, 0);
		return true;
	}

	/**
	 * A {@link VirtualKeyboard} instance to show or hide the virtual keyboard
	 * programmatically, and get the current position and size of the virtual keyboard.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Navigator/virtualKeyboard
	 */
	get virtualKeyboard() {
		let vk = state.vk;
		if (!vk) vk = state.vk = newVirtualKeyboard();
		return vk;
	}
}
def('Navigator', Navigator);

// @ts-expect-error Internal constructor
export const navigator = new Navigator(INTERNAL_SYMBOL);
def('navigator', navigator);

function processVibrations() {
	let next = state.vibrationPattern?.shift();
	if (typeof next === 'undefined') {
		// Pattern completed
		next = 0;
	}
	if (typeof next === 'number') {
		// Pause interval
		$.hidSendVibrationValues(STOP_VIBRATION);
		if (next > 0) {
			state.vibrationTimeoutId = setTimeout(processVibrations, next);
		}
	} else {
		// Vibration interval
		$.hidSendVibrationValues(next);
		state.vibrationTimeoutId = setTimeout(processVibrations, next.duration);
	}
}
