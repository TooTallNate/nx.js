import { $ } from '../$';
import { INTERNAL_SYMBOL } from '../internal';
import { IllegalConstructor, def } from '../utils';
import type { SwitchClass } from '../switch';
import type { Navigator } from '../navigator';

declare const Switch: SwitchClass;

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
		if (arguments[0] !== INTERNAL_SYMBOL) {
			throw new IllegalConstructor();
		}
		super();
		$.batteryInit();
		Switch.addEventListener('exit', $.batteryExit);
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
