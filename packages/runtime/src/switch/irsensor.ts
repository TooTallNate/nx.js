import { $ } from '../$';
import { ImageBitmap } from '../canvas/image-bitmap';
import { INTERNAL_SYMBOL } from '../internal';
import { Event } from '../polyfills/event';
import { Sensor } from '../sensor';
import { clearInterval, setInterval } from '../timers';
import { createInternal } from '../utils';

let init = false;

interface IRSensorInternal {
	activated: boolean;
	timestamp: number | null;
	frequency: number;
	image: ImageBitmap;
	interval?: number;
}

const _ = createInternal<IRSensor, IRSensorInternal>();

export interface IRSensorInit {
	/**
	 * The desired number of times per second a sample should be taken,
	 * meaning the number of times per second that the `reading` event
	 * will be called. A whole number or decimal may be used, the latter
	 * for frequencies less than a second. The actual reading frequency
	 * depends on the device hardware and consequently may be less than
	 * requested.
	 */
	frequency?: number;
}

/**
 * The `IRSensor` class is a `Sensor` subclass. When the sensor is
 * activated, the controller's IR (infrared) camera is enabled,
 * allowing the application to get the image data for each frame of
 * the camera.
 *
 * @example
 *
 * ```typescript
 * const ctx = screen.getContext('2d');
 *
 * const sensor = new Switch.IRSensor();
 * sensor.addEventListener('reading', () => {
 * 	ctx.drawImage(sensor.image, 0, 0);
 * });
 * sensor.start();
 * ```
 */
export class IRSensor extends Sensor {
	/**
	 * @ignore
	 */
	constructor(opts: IRSensorInit = {}) {
		if (!init) {
			init = true;
			addEventListener('unload', $.irsInit());
		}
		// @ts-expect-error Internal constructor
		super(INTERNAL_SYMBOL);
		const image = $.imageNew(320, 240) as ImageBitmap;
		Object.setPrototypeOf(image, ImageBitmap.prototype);
		const self = $.irsSensorNew(image);
		Object.setPrototypeOf(self, IRSensor.prototype);
		_.set(self, {
			activated: false,
			timestamp: null,
			image,
			frequency: opts.frequency ?? 2,
		});
		return self;
	}

	/**
	 * The underlying `ImageBitmap` instance containing the contents
	 * of the IR sensor. Can be used with `ctx.drawImage()` or any
	 * other functions that work with `ImageBitmap` instances.
	 */
	get image() {
		return _(this).image;
	}

	get activated(): boolean {
		return _(this).activated;
	}

	get hasReading(): boolean {
		return _(this).timestamp !== null;
	}

	get timestamp(): number | null {
		return _(this).timestamp;
	}

	start(): void {
		const i = _(this);
		if (!i.activated) {
			i.activated = true;
			this.dispatchEvent(new Event('activate'));
		}
		this.stop();
		$.irsSensorStart(this);
		i.interval = setInterval(() => {
			if ($.irsSensorUpdate(this)) {
				i.timestamp = Date.now();
				this.dispatchEvent(new Event('reading'));
			}
		}, 1000 / i.frequency);
	}

	stop(): void {
		clearInterval(_(this).interval);
		$.irsSensorStop(this);
	}
}
