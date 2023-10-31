import { IllegalConstructor, def } from './utils';
import { BatteryManager } from './navigator/battery';
import { INTERNAL_SYMBOL } from './types';
import type { SwitchClass } from './switch';
import {
	type VirtualKeyboard,
	create as newVirtualKeyboard,
} from './navigator/virtual-keyboard';

declare const Switch: SwitchClass;

interface NavigatorState {
	batt?: BatteryManager;
	vk?: VirtualKeyboard;
}

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
		throw new IllegalConstructor();
	}

	/**
	 * Returns the value used for the `User-Agent` HTTP request header for
	 * HTTP requests initiated with {@link fetch | `fetch()`}.
	 *
	 * @example "my-app/0.0.1 (Switch; en-us; rv:14.1.2|AMS 1.5.4|E) nx.js/0.0.18"
	 * @see https://developer.mozilla.org/docs/Web/API/Navigator/userAgent
	 */
	get userAgent() {
		return `nx.js/${Switch.version.nxjs}`;
	}

	/**
	 * Returns the maximum number of simultaneous touch contact points are
	 * supported by the current device.
	 *
	 * @example 10
	 * @see https://developer.mozilla.org/docs/Web/API/Navigator/maxTouchPoints
	 */
	get maxTouchPoints() {
		return 10;
	}

	/**
	 * Returns a promise which is resolved to a {@link BatteryManager} instance.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Navigator/getBattery
	 */
	async getBattery() {
		let b = state.batt;
		if (!b) {
			// @ts-expect-error
			b = state.batt = new BatteryManager(INTERNAL_SYMBOL);
		}
		return b;
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

export const navigator: Navigator = Object.create(Navigator.prototype);
def('navigator', navigator);
