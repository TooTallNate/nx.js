import { $ } from './$';
import { INTERNAL_SYMBOL } from './internal';
import { Event } from './polyfills/event';
import { Sensor } from './sensor';
import { setInterval, clearInterval } from './timers';
import { createInternal, def } from './utils';

export interface AmbientLightSensorOptions {
	/**
	 * The desired number of times per second a sample should be taken,
	 * meaning the number of times per second that the `reading` event
	 * will be called. A whole number or decimal may be used, the latter
	 * for frequencies less than a second.
	 */
	frequency?: number;
}

interface AmbientLightSensorInternal {
	frequency: number;
	timeout?: number;
	illuminance: number | null;
	timestamp: number | null;
}

const _ = createInternal<AmbientLightSensor, AmbientLightSensorInternal>();

/**
 * `Sensor` implementation which returns the current light level or
 * illuminance of the ambient light around the hosting device.
 *
 * @see https://developer.mozilla.org/docs/Web/API/AmbientLightSensor
 */
export class AmbientLightSensor extends Sensor {
	/**
	 * @see https://developer.mozilla.org/docs/Web/API/AmbientLightSensor/AmbientLightSensor
	 */
	constructor(opts: AmbientLightSensorOptions = {}) {
		// @ts-expect-error Internal constructor
		super(INTERNAL_SYMBOL);
		_.set(this, {
			frequency: opts.frequency ?? 1,
			illuminance: null,
			timestamp: null,
		});
	}

	/**
	 * The current light level (in {@link https://wikipedia.org/wiki/Lux | lux}) of the ambient light level around the hosting device.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/AmbientLightSensor/illuminance
	 */
	get illuminance() {
		return _(this).illuminance;
	}

	get activated(): boolean {
		return typeof _(this).timeout === 'number';
	}

	get hasReading(): boolean {
		return typeof _(this).illuminance === 'number';
	}

	get timestamp() {
		return _(this).timestamp;
	}

	start(): void {
		const i = _(this);
		i.timeout = setInterval(() => {
			const prev = i.illuminance;
			i.illuminance = $.appletIlluminance();
			if (i.illuminance !== prev) {
				i.timestamp = Date.now();
				this.dispatchEvent(new Event('reading'));
			}
		}, 1000 / i.frequency);
	}

	stop(): void {
		clearInterval(_(this).timeout);
	}
}
def('AmbientLightSensor', AmbientLightSensor);
