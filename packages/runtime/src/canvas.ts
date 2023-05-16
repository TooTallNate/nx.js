import colorRgba = require('color-rgba');
import type { CanvasRenderingContext2DState, Switch } from './switch';
import { INTERNAL_SYMBOL } from './switch';
import { LibImageData } from './types';

export class Canvas {
	[INTERNAL_SYMBOL]: Switch;
	width: number;
	height: number;

	constructor(s: Switch, width: number, height: number) {
		this[INTERNAL_SYMBOL] = s;
		this.width = width;
		this.height = height;
	}

	getContext(contextId: '2d'): CanvasRenderingContext2D {
		if (contextId !== '2d') {
			throw new TypeError(
				`"${contextId}" is not supported. Must be "2d".`
			);
		}
		return new CanvasRenderingContext2D(this);
	}
}

export class ImageData implements LibImageData {
	readonly colorSpace: PredefinedColorSpace;
	readonly data: Uint8ClampedArray;
	readonly height: number;
	readonly width: number;

	constructor(sw: number, sh: number, settings?: ImageDataSettings);
	constructor(
		data: Uint8ClampedArray,
		sw: number,
		sh?: number,
		settings?: ImageDataSettings
	);
	constructor(
		swOrData: number | Uint8ClampedArray,
		shOrSw: number,
		settingsOrSh?: ImageDataSettings | number,
		settings?: ImageDataSettings
	) {
		let imageDataSettings: ImageDataSettings | undefined;
		if (typeof swOrData === 'number') {
			this.width = swOrData;
			this.height = shOrSw;
			if (typeof settingsOrSh !== 'number') {
				imageDataSettings = settingsOrSh;
			}
			this.data = new Uint8ClampedArray(this.width * this.height * 4);
		} else {
			this.data = swOrData;
			this.width = shOrSw;
			if (typeof settingsOrSh === 'number') {
				this.height = settingsOrSh;
			} else {
				this.height = (this.data.length / this.width) | 0;
			}
			imageDataSettings = settings;
		}
		this.colorSpace = imageDataSettings?.colorSpace || 'srgb';
	}
}

export class CanvasRenderingContext2D
	implements CanvasImageData, CanvasRect, CanvasText, CanvasFillStrokeStyles
{
	readonly canvas: Canvas;
	[INTERNAL_SYMBOL]: CanvasRenderingContext2DState;

	strokeStyle: string | CanvasGradient | CanvasPattern;

	constructor(canvas: Canvas) {
		const { native } = canvas[INTERNAL_SYMBOL];
		const { width: w, height: h } = canvas;
		this.canvas = canvas;
		this[INTERNAL_SYMBOL] = native.canvasNewContext(w, h);
		this.strokeStyle = '';
	}

	get fillStyle(): string | CanvasGradient | CanvasPattern {
		return '';
	}

	set fillStyle(v: string | CanvasGradient | CanvasPattern) {
		if (typeof v === 'string') {
			const parsed = colorRgba(v);
			if (!parsed || parsed.length !== 4) {
				return;
			}
			const { native } = this.canvas[INTERNAL_SYMBOL];
			native.canvasSetFillStyle(this[INTERNAL_SYMBOL], ...parsed);
		} else {
			throw new Error('CanvasGradient/CanvasPattern not implemented.');
		}
	}

	fillText(
		text: string,
		x: number,
		y: number,
		maxWidth?: number | undefined
	): void {
		throw new Error('Method not implemented.');
	}

	measureText(text: string): TextMetrics {
		throw new Error('Method not implemented.');
	}

	strokeText(
		text: string,
		x: number,
		y: number,
		maxWidth?: number | undefined
	): void {
		throw new Error('Method not implemented.');
	}

	createConicGradient(
		startAngle: number,
		x: number,
		y: number
	): CanvasGradient {
		throw new Error('Method not implemented.');
	}
	createLinearGradient(
		x0: number,
		y0: number,
		x1: number,
		y1: number
	): CanvasGradient {
		throw new Error('Method not implemented.');
	}
	createPattern(
		image: CanvasImageSource,
		repetition: string | null
	): CanvasPattern | null {
		throw new Error('Method not implemented.');
	}
	createRadialGradient(
		x0: number,
		y0: number,
		r0: number,
		x1: number,
		y1: number,
		r1: number
	): CanvasGradient {
		throw new Error('Method not implemented.');
	}

	clearRect(x: number, y: number, w: number, h: number): void {
		throw new Error('Method not implemented.');
	}

	fillRect(x: number, y: number, w: number, h: number): void {
		const { native } = this.canvas[INTERNAL_SYMBOL];
		native.canvasFillRect(this[INTERNAL_SYMBOL], x, y, w, h);
	}

	strokeRect(x: number, y: number, w: number, h: number): void {
		throw new Error('Method not implemented.');
	}

	createImageData(
		sw: number,
		sh: number,
		settings?: ImageDataSettings
	): ImageData;
	createImageData(imagedata: ImageData): ImageData;
	createImageData(
		sw: number | ImageData,
		sh?: number,
		settings?: ImageDataSettings
	): ImageData {
		let width: number;
		let height: number;
		if (typeof sw === 'number') {
			if (typeof sh !== 'number') {
				throw new TypeError(
					'CanvasRenderingContext2D.createImageData: Argument 1 is not an object.'
				);
			}
			width = sw;
			height = sh;
		} else {
			width = sw.width;
			height = sw.height;
		}
		if (width <= 0 || height <= 0) {
			throw new TypeError(
				'CanvasRenderingContext2D.createImageData: Invalid width or height'
			);
		}
		return new ImageData(width, height, settings);
	}

	getImageData(
		sx: number,
		sy: number,
		sw: number,
		sh: number,
		settings?: ImageDataSettings | undefined
	): ImageData {
		const { native } = this.canvas[INTERNAL_SYMBOL];
		const buffer = native.canvasGetImageData(
			this[INTERNAL_SYMBOL],
			sx,
			sy,
			sw,
			sh
		);
		const data = new Uint8ClampedArray(buffer);
		return new ImageData(data, sw, sh, settings);
	}

	putImageData(
		imagedata: ImageData,
		dx: number,
		dy: number,
		dirtyX = 0,
		dirtyY = 0,
		dirtyWidth = imagedata.width,
		dirtyHeight = imagedata.height
	): void {
		const { native } = this.canvas[INTERNAL_SYMBOL];
		native.canvasPutImageData(
			this[INTERNAL_SYMBOL],
			imagedata.data.buffer,
			dx,
			dy,
			dirtyX,
			dirtyY,
			dirtyWidth,
			dirtyHeight
		);
	}
}
