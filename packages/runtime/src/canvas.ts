import toPx = require('to-px/index.js');
import colorRgba = require('color-rgba');
import parseCssFont from 'parse-css-font';
import { INTERNAL_SYMBOL } from './types';
import type { CanvasRenderingContext2DState } from './switch';
import type { Switch as _Switch } from './switch';

declare const Switch: _Switch;

export class Canvas {
	width: number;
	height: number;

	constructor(width: number, height: number) {
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

export class ImageData implements globalThis.ImageData {
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
	implements
		CanvasImageData,
		CanvasRect,
		CanvasText,
		CanvasDrawPath,
		CanvasPath,
		CanvasPathDrawingStyles,
		CanvasFillStrokeStyles,
		CanvasTextDrawingStyles,
		CanvasTransform
{
	readonly canvas: Canvas;
	[INTERNAL_SYMBOL]: CanvasRenderingContext2DState;

	strokeStyle: string | CanvasGradient | CanvasPattern;
	direction: CanvasDirection;
	fontKerning: CanvasFontKerning;
	textAlign: CanvasTextAlign;
	textBaseline: CanvasTextBaseline;

	// TODO
	lineCap!: CanvasLineCap;
	lineDashOffset!: number;
	lineJoin!: CanvasLineJoin;
	miterLimit!: number;

	constructor(canvas: Canvas) {
		const { width: w, height: h } = canvas;
		this.canvas = canvas;
		this[INTERNAL_SYMBOL] = Switch.native.canvasNewContext(w, h);
		this.font = '24pt system-ui';

		// TODO: implement
		this.strokeStyle = '';
		this.direction = 'ltr';
		this.fontKerning = 'auto';
		this.textAlign = 'left';
		this.textBaseline = 'alphabetic';
	}
	getTransform(): DOMMatrix {
		throw new Error('Method not implemented.');
	}
	resetTransform(): void {
		throw new Error('Method not implemented.');
	}
	rotate(angle: number): void {
		Switch.native.canvasRotate(this[INTERNAL_SYMBOL], angle);
	}
	scale(x: number, y: number): void {
		Switch.native.canvasScale(this[INTERNAL_SYMBOL], x, y);
	}
	setTransform(
		a: number,
		b: number,
		c: number,
		d: number,
		e: number,
		f: number
	): void;
	setTransform(transform?: DOMMatrix2DInit): void;
	setTransform(
		a?: DOMMatrix2DInit | number,
		b?: number,
		c?: number,
		d?: number,
		e?: number,
		f?: number
	): void {
		throw new Error('Method not implemented.');
	}
	transform(
		a: number,
		b: number,
		c: number,
		d: number,
		e: number,
		f: number
	): void {
		Switch.native.canvasTransform(this[INTERNAL_SYMBOL], a, b, c, d, e, f);
	}

	translate(x: number, y: number): void {
		Switch.native.canvasTranslate(this[INTERNAL_SYMBOL], x, y);
	}

	arc(
		x: number,
		y: number,
		radius: number,
		startAngle: number,
		endAngle: number,
		counterclockwise?: boolean | undefined
	): void {
		throw new Error('Method not implemented.');
	}
	arcTo(
		x1: number,
		y1: number,
		x2: number,
		y2: number,
		radius: number
	): void {
		throw new Error('Method not implemented.');
	}
	bezierCurveTo(
		cp1x: number,
		cp1y: number,
		cp2x: number,
		cp2y: number,
		x: number,
		y: number
	): void {
		throw new Error('Method not implemented.');
	}
	closePath(): void {
		Switch.native.canvasClosePath(this[INTERNAL_SYMBOL]);
	}
	ellipse(
		x: number,
		y: number,
		radiusX: number,
		radiusY: number,
		rotation: number,
		startAngle: number,
		endAngle: number,
		counterclockwise?: boolean | undefined
	): void {
		throw new Error('Method not implemented.');
	}
	lineTo(x: number, y: number): void {
		Switch.native.canvasLineTo(this[INTERNAL_SYMBOL], x, y);
	}
	moveTo(x: number, y: number): void {
		Switch.native.canvasMoveTo(this[INTERNAL_SYMBOL], x, y);
	}
	quadraticCurveTo(cpx: number, cpy: number, x: number, y: number): void {
		throw new Error('Method not implemented.');
	}
	rect(x: number, y: number, w: number, h: number): void {
		Switch.native.canvasRect(this[INTERNAL_SYMBOL], x, y, w, h);
	}
	roundRect(
		x: number,
		y: number,
		w: number,
		h: number,
		radii?: number | DOMPointInit | (number | DOMPointInit)[]
	): void;
	roundRect(
		x: number,
		y: number,
		w: number,
		h: number,
		radii?: number | DOMPointInit | Iterable<number | DOMPointInit>
	): void;
	roundRect(
		x: number,
		y: number,
		w: number,
		h: number,
		radii?: number | DOMPointInit | Iterable<number | DOMPointInit>
	): void {
		throw new Error('Method not implemented.');
	}
	beginPath(): void {
		Switch.native.canvasBeginPath(this[INTERNAL_SYMBOL]);
	}
	clip(fillRule?: CanvasFillRule | undefined): void;
	clip(path: Path2D, fillRule?: CanvasFillRule | undefined): void;
	clip(path?: unknown, fillRule?: unknown): void {
		throw new Error('Method not implemented.');
	}
	fill(fillRule?: CanvasFillRule): void;
	fill(path: Path2D, fillRule?: CanvasFillRule): void;
	fill(
		fillRuleOrPath?: CanvasFillRule | Path2D,
		fillRule?: CanvasFillRule
	): void {
		Switch.native.canvasFill(this[INTERNAL_SYMBOL]);
	}
	isPointInPath(
		x: number,
		y: number,
		fillRule?: CanvasFillRule | undefined
	): boolean;
	isPointInPath(
		path: Path2D,
		x: number,
		y: number,
		fillRule?: CanvasFillRule | undefined
	): boolean;
	isPointInPath(
		pathOrX: Path2D | number,
		xOrY: number,
		yOrFillRule?: number | CanvasFillRule,
		fillRule?: CanvasFillRule
	): boolean {
		throw new Error('Method not implemented.');
	}
	isPointInStroke(x: number, y: number): boolean;
	isPointInStroke(path: Path2D, x: number, y: number): boolean;
	isPointInStroke(path: unknown, x: unknown, y?: unknown): boolean {
		throw new Error('Method not implemented.');
	}
	stroke(path?: Path2D): void {
		Switch.native.canvasStroke(this[INTERNAL_SYMBOL]);
	}

	getLineDash(): number[] {
		throw new Error('Method not implemented.');
	}

	setLineDash(segments: number[]): void;
	setLineDash(segments: Iterable<number>): void;
	setLineDash(segments: Iterable<number>): void {
		throw new Error('Method not implemented.');
	}

	get lineWidth(): number {
		throw new Error('Method not implemented.');
	}

	set lineWidth(v: number) {
		Switch.native.canvasSetLineWidth(this[INTERNAL_SYMBOL], v);
	}

	get fillStyle(): string | CanvasGradient | CanvasPattern {
		// TODO: implement
		return '';
	}

	set fillStyle(v: string | CanvasGradient | CanvasPattern) {
		if (typeof v === 'string') {
			const parsed = colorRgba(v);
			if (!parsed || parsed.length !== 4) {
				return;
			}
			Switch.native.canvasSetFillStyle(this[INTERNAL_SYMBOL], ...parsed);
		} else {
			throw new Error('CanvasGradient/CanvasPattern not implemented.');
		}
	}

	get font(): string {
		// TODO: implement
		return '';
	}

	set font(v: string) {
		const parsed = parseCssFont(v);
		if ('system' in parsed) {
			// "system" fonts are not supported
			return;
		}
		if (typeof parsed.size !== 'string') {
			// Missing required font size
			return;
		}
		if (!parsed.family) {
			// Missing required font name
			return;
		}
		const px = toPx(parsed.size);
		if (typeof px !== 'number') {
			// Invalid font size
			return;
		}
		const { native, fonts } = Switch;
		let font = fonts._findFont(parsed);
		if (!font) {
			if (parsed.family.includes('system-ui')) {
				font = fonts._addSystemFont();
			} else {
				return;
			}
		}
		native.canvasSetFont(
			this[INTERNAL_SYMBOL],
			font[INTERNAL_SYMBOL].fontFace,
			px
		);
	}

	fillText(
		text: string,
		x: number,
		y: number,
		maxWidth?: number | undefined
	): void {
		Switch.native.canvasFillText(
			this[INTERNAL_SYMBOL],
			text,
			x,
			y,
			maxWidth
		);
	}

	measureText(text: string): TextMetrics {
		return Switch.native.canvasMeasureText(this[INTERNAL_SYMBOL], text);
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
		Switch.native.canvasFillRect(this[INTERNAL_SYMBOL], x, y, w, h);
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
		const buffer = Switch.native.canvasGetImageData(
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
		Switch.native.canvasPutImageData(
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
