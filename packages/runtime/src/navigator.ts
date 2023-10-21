import { def } from './utils';
import { BatteryManager } from './navigator/battery';
import { INTERNAL_SYMBOL } from './types';
import type { SwitchClass } from './switch';

declare const Switch: SwitchClass;

interface NavigatorState {
	battery?: BatteryManager;
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
		throw new TypeError('Illegal constructor.');
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
	 * Returns a battery promise, which is resolved in a {@link BatteryManager} object
	 * providing also some new events you can handle to monitor the battery status.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Navigator/getBattery
	 */
	async getBattery() {
		let b = state.battery;
		if (!b) {
			// @ts-expect-error
			b = state.battery = new BatteryManager(INTERNAL_SYMBOL);
		}
		return b;
	}
}
def('Navigator', Navigator);

export const navigator: Navigator = Object.create(Navigator.prototype);
def('navigator', navigator);
