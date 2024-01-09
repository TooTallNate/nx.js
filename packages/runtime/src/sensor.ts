import { EventTarget } from './polyfills/event-target';
import { assertInternalConstructor, def } from './utils';

/**
 * `Sensor` is the base class for all the other sensor interfaces.
 * This interface cannot be used directly. Instead it provides properties,
 * event handlers, and methods accessed by interfaces that inherit from it.
 *
 * @see https://developer.mozilla.org/docs/Web/API/Sensor
 */
export abstract class Sensor extends EventTarget {
	/**
	 * @ignore
	 */
	constructor() {
		super();
		assertInternalConstructor(arguments);
	}

	/**
	 * A read-only boolean value indicating whether the sensor is active.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Sensor/activated
	 */
	abstract get activated(): boolean;

	/**
	 * A read-only boolean value indicating whether the sensor has a reading.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Sensor/hasReading
	 */
	abstract get hasReading(): boolean;

	/**
	 * A read-only number representing the timestamp of the latest sensor reading.
	 * Value is `null` if there has not yet been a reading of the sensor.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Sensor/timestamp
	 */
	abstract get timestamp(): number | null;

	/**
	 * Activates the sensor.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Sensor/start
	 */
	abstract start(): void;

	/**
	 * Deactivates the sensor.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/Sensor/stop
	 */
	abstract stop(): void;

	addEventListener(
		type: 'activate' | 'error' | 'reading',
		listener: (ev: Event) => any,
		options?: boolean | AddEventListenerOptions
	): void;
	addEventListener(
		type: string,
		listener: EventListenerOrEventListenerObject,
		options?: boolean | AddEventListenerOptions
	): void;
	addEventListener(
		type: string,
		callback: EventListenerOrEventListenerObject | null,
		options?: boolean | AddEventListenerOptions
	): void {
		super.addEventListener(type, callback, options);
	}
}
def(Sensor);
