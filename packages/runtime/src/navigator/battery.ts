import { $ } from '../$';
import { assertInternalConstructor, def } from '../utils';
import { EventTarget } from '../polyfills/event-target';
import type { Navigator } from '../navigator';

/**
 * Provides information about the system's battery charge level.
 * The {@link Navigator.getBattery | `navigator.getBattery()`} method
 * returns a promise that resolves to a `BatteryManager` instance.
 *
 * @see https://developer.mozilla.org/docs/Web/API/BatteryManager
 */
export class BatteryManager extends EventTarget {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
		super();
		$.batteryInit();
		addEventListener('unload', $.batteryExit);
	}

	/**
	 * A number representing the system's battery charge level scaled
	 * to a value between 0.0 and 1.0.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BatteryManager/level
	 */
	declare readonly level: number;

	/**
	 * A Boolean value indicating whether the battery is currently being charged.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/BatteryManager/charging
	 */
	declare readonly charging: boolean;

	get chargingTime() {
		return Infinity;
	}

	get dischargingTime() {
		return Infinity;
	}
}
$.batteryInitClass(BatteryManager);
def('BatteryManager', BatteryManager);
