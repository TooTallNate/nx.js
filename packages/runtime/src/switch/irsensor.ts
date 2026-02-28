import colorRgba = require('color-rgba');
import { $ } from '../$';
import { ImageBitmap } from '../canvas/image-bitmap';
import { INTERNAL_SYMBOL } from '../internal';
import { Event } from '../polyfills/event';
import { Sensor } from '../sensor';
import { clearInterval, setInterval } from '../timers';
import { createInternal, proto } from '../utils';

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
	 * for frequencies less than a second.
	 *
	 * @default 2
	 */
	frequency?: number;
	/**
	 * CSS color that will be used when rendering the image produced by
	 * the IR sensor.
	 *
	 * @default "green"
	 */
	color?: string;
}

/**
 * The `IRSensor` class reads the controller's IR (infrared) camera,
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
	constructor(opts: IRSensorInit = {}) {
		if (!init) {
			init = true;
			addEventListener('unload', $.irsInit());
		}
		const color = colorRgba(opts.color || 'green');
		if (!color || color.length !== 4) {
			throw new Error(`Invalid color specified: "${opts.color}"`);
		}
		// @ts-expect-error Internal constructor
		super(INTERNAL_SYMBOL);
		const image = proto($.imageNew(320, 240), ImageBitmap);
		const self = proto($.irsSensorNew(image, color), IRSensor);
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
