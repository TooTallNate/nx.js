import { $ } from './$';
import { INTERNAL_SYMBOL } from './internal';
import { Event } from './polyfills/event';
import { Sensor } from './sensor';
import { clearInterval, setInterval } from './timers';
import { def } from './utils';

export interface AmbientLightSensorOptions {
	/**
	 * The desired number of times per second a sample should be taken,
	 * meaning the number of times per second that the `reading` event
	 * will be called. A whole number or decimal may be used, the latter
	 * for frequencies less than a second.
	 */
	frequency?: number;
}

/**
 * `Sensor` implementation which returns the current light level or
 * illuminance of the ambient light around the hosting device.
 *
 * @see https://developer.mozilla.org/docs/Web/API/AmbientLightSensor
 */
export class AmbientLightSensor extends Sensor {
	#frequency: number;
	#timeout?: number;
	#illuminance: number | null;
	#timestamp: number | null;

	/**
	 * @see https://developer.mozilla.org/docs/Web/API/AmbientLightSensor/AmbientLightSensor
	 */
	constructor(opts: AmbientLightSensorOptions = {}) {
		// @ts-expect-error Internal constructor
		super(INTERNAL_SYMBOL);
		this.#frequency = opts.frequency ?? 1;
		this.#illuminance = null;
		this.#timestamp = null;
	}

	/**
	 * The current light level (in {@link https://wikipedia.org/wiki/Lux | lux}) of the ambient light level around the hosting device.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AmbientLightSensor/illuminance
	 */
	get illuminance() {
		return this.#illuminance;
	}

	get activated(): boolean {
		return typeof this.#timeout === 'number';
	}

	get hasReading(): boolean {
		return typeof this.#illuminance === 'number';
	}

	get timestamp() {
		return this.#timestamp;
	}

	start(): void {
		this.#timeout = setInterval(() => {
			const prev = this.#illuminance;
			this.#illuminance = $.appletIlluminance();
			if (this.#illuminance !== prev) {
				this.#timestamp = Date.now();
				this.dispatchEvent(new Event('reading'));
			}
		}, 1000 / this.#frequency);
	}

	stop(): void {
		clearInterval(this.#timeout);
	}
}
def(AmbientLightSensor);
