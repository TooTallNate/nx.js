import toPx = require('to-px/index.js');
import colorRgba = require('color-rgba');
import parseCssFont from 'parse-css-font';
import { INTERNAL_SYMBOL } from './types';
import type { CanvasRenderingContext2DState } from './switch';
import type { Switch as _Switch } from './switch';

declare const Switch: _Switch;

type RGBA = [number, number, number, number];

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
		CanvasDrawPath,
		CanvasFillStrokeStyles,
		CanvasImageData,
		CanvasPath,
		CanvasPathDrawingStyles,
		CanvasRect,
		CanvasState,
		CanvasText,
		CanvasTextDrawingStyles,
		CanvasTransform
{
	readonly canvas: Canvas;
	[INTERNAL_SYMBOL]: {
		ctx: CanvasRenderingContext2DState;
		fillStyle: RGBA;
		strokeStyle: RGBA;
		currentStyle?: RGBA;
	};

	direction: CanvasDirection;
	fontKerning: CanvasFontKerning;
	textAlign: CanvasTextAlign;
	textBaseline: CanvasTextBaseline;

	// TODO
	miterLimit!: number;

	constructor(canvas: Canvas) {
		const { width: w, height: h } = canvas;
		this.canvas = canvas;
		this[INTERNAL_SYMBOL] = {
			ctx: Switch.native.canvasNewContext(w, h),
			strokeStyle: [0, 0, 0, 1],
			fillStyle: [0, 0, 0, 1],
		};
		this.font = '10px system-ui';

		// TODO: implement
		this.direction = 'ltr';
		this.fontKerning = 'auto';
		this.textAlign = 'left';
		this.textBaseline = 'alphabetic';
	}

	restore(): void {
		throw new Error('Method not implemented.');
	}

	save(): void {
		throw new Error('Method not implemented.');
	}

	rotate(angle: number): void {
		Switch.native.canvasRotate(this[INTERNAL_SYMBOL].ctx, angle);
	}

	scale(x: number, y: number): void {
		Switch.native.canvasScale(this[INTERNAL_SYMBOL].ctx, x, y);
	}

	getTransform(): DOMMatrix {
		return new DOMMatrix(
			Switch.native.canvasGetTransform(this[INTERNAL_SYMBOL].ctx)
		);
	}

	resetTransform(): void {
		Switch.native.canvasResetTransform(this[INTERNAL_SYMBOL].ctx);
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
		this.resetTransform();
		if (typeof a === 'object') {
			b = a.b;
			c = a.c;
			d = a.d;
			e = a.e;
			f = a.f;
			a = a.a;
		}
		this.transform(a ?? 0, b ?? 0, c ?? 0, d ?? 0, e ?? 0, f ?? 0);
	}

	transform(
		a: number,
		b: number,
		c: number,
		d: number,
		e: number,
		f: number
	): void {
		Switch.native.canvasTransform(
			this[INTERNAL_SYMBOL].ctx,
			a,
			b,
			c,
			d,
			e,
			f
		);
	}

	translate(x: number, y: number): void {
		Switch.native.canvasTranslate(this[INTERNAL_SYMBOL].ctx, x, y);
	}

	arc(
		x: number,
		y: number,
		radius: number,
		startAngle: number,
		endAngle: number,
		counterclockwise?: boolean
	): void {
		Switch.native.canvasArc(
			this[INTERNAL_SYMBOL].ctx,
			x,
			y,
			radius,
			startAngle,
			endAngle,
			counterclockwise ?? false
		);
	}

	arcTo(
		x1: number,
		y1: number,
		x2: number,
		y2: number,
		radius: number
	): void {
		Switch.native.canvasArcTo(
			this[INTERNAL_SYMBOL].ctx,
			x1,
			y1,
			x2,
			y2,
			radius
		);
	}

	bezierCurveTo(
		cp1x: number,
		cp1y: number,
		cp2x: number,
		cp2y: number,
		x: number,
		y: number
	): void {
		Switch.native.canvasBezierCurveTo(
			this[INTERNAL_SYMBOL].ctx,
			cp1x,
			cp1y,
			cp2x,
			cp2y,
			x,
			y
		);
	}

	quadraticCurveTo(cpx: number, cpy: number, x: number, y: number): void {
		Switch.native.canvasQuadraticCurveTo(
			this[INTERNAL_SYMBOL].ctx,
			cpx,
			cpy,
			x,
			y
		);
	}

	closePath(): void {
		Switch.native.canvasClosePath(this[INTERNAL_SYMBOL].ctx);
	}

	ellipse(
		x: number,
		y: number,
		radiusX: number,
		radiusY: number,
		rotation: number,
		startAngle: number,
		endAngle: number,
		counterclockwise?: boolean
	): void {
		Switch.native.canvasEllipse(
			this[INTERNAL_SYMBOL].ctx,
			x,
			y,
			radiusX,
			radiusY,
			rotation,
			startAngle,
			endAngle,
			counterclockwise ?? false
		);
	}

	lineTo(x: number, y: number): void {
		Switch.native.canvasLineTo(this[INTERNAL_SYMBOL].ctx, x, y);
	}

	moveTo(x: number, y: number): void {
		Switch.native.canvasMoveTo(this[INTERNAL_SYMBOL].ctx, x, y);
	}

	rect(x: number, y: number, w: number, h: number): void {
		Switch.native.canvasRect(this[INTERNAL_SYMBOL].ctx, x, y, w, h);
	}

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
		Switch.native.canvasBeginPath(this[INTERNAL_SYMBOL].ctx);
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
		if (fillRuleOrPath) {
			throw new Error('`path` param not implemented.');
		}
		const i = this[INTERNAL_SYMBOL];
		if (i.currentStyle !== i.fillStyle) {
			Switch.native.canvasSetSourceRgba(i.ctx, ...i.fillStyle);
		}
		Switch.native.canvasFill(i.ctx);
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
		if (path) {
			throw new Error('`path` param not implemented.');
		}
		const i = this[INTERNAL_SYMBOL];
		if (i.currentStyle !== i.strokeStyle) {
			Switch.native.canvasSetSourceRgba(i.ctx, ...i.strokeStyle);
		}
		Switch.native.canvasStroke(i.ctx);
	}

	getLineDash(): number[] {
		return Switch.native.canvasGetLineDash(this[INTERNAL_SYMBOL].ctx);
	}

	setLineDash(segments: Iterable<number>): void {
		Switch.native.canvasSetLineDash(
			this[INTERNAL_SYMBOL].ctx,
			Array.from(segments)
		);
	}

	get lineDashOffset() {
		return Switch.native.canvasGetLineDashOffset(this[INTERNAL_SYMBOL].ctx);
	}

	set lineDashOffset(v: number) {
		Switch.native.canvasSetLineDashOffset(this[INTERNAL_SYMBOL].ctx, v);
	}

	get lineWidth() {
		return Switch.native.canvasGetLineWidth(this[INTERNAL_SYMBOL].ctx);
	}

	set lineWidth(v: number) {
		Switch.native.canvasSetLineWidth(this[INTERNAL_SYMBOL].ctx, v);
	}

	get lineJoin() {
		return Switch.native.canvasGetLineJoin(this[INTERNAL_SYMBOL].ctx);
	}

	set lineJoin(v: CanvasLineJoin) {
		Switch.native.canvasSetLineJoin(this[INTERNAL_SYMBOL].ctx, v);
	}

	get lineCap() {
		return Switch.native.canvasGetLineCap(this[INTERNAL_SYMBOL].ctx);
	}

	set lineCap(v: CanvasLineCap) {
		Switch.native.canvasSetLineCap(this[INTERNAL_SYMBOL].ctx, v);
	}

	get fillStyle(): string | CanvasGradient | CanvasPattern {
		return rgbaToString(this[INTERNAL_SYMBOL].fillStyle);
	}

	set fillStyle(v: string | CanvasGradient | CanvasPattern) {
		if (typeof v === 'string') {
			const parsed = colorRgba(v);
			if (!parsed || parsed.length !== 4) {
				return;
			}
			this[INTERNAL_SYMBOL].fillStyle = parsed;
		} else {
			throw new Error('CanvasGradient/CanvasPattern not implemented.');
		}
	}

	get strokeStyle(): string | CanvasGradient | CanvasPattern {
		return rgbaToString(this[INTERNAL_SYMBOL].strokeStyle);
	}

	set strokeStyle(v: string | CanvasGradient | CanvasPattern) {
		if (typeof v === 'string') {
			const parsed = colorRgba(v);
			if (!parsed || parsed.length !== 4) {
				return;
			}
			this[INTERNAL_SYMBOL].strokeStyle = parsed;
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
			this[INTERNAL_SYMBOL].ctx,
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
		const i = this[INTERNAL_SYMBOL];
		if (i.currentStyle !== i.fillStyle) {
			Switch.native.canvasSetSourceRgba(i.ctx, ...i.fillStyle);
		}
		Switch.native.canvasFillText(i.ctx, text, x, y, maxWidth);
	}

	measureText(text: string): TextMetrics {
		return Switch.native.canvasMeasureText(this[INTERNAL_SYMBOL].ctx, text);
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
		const i = this[INTERNAL_SYMBOL];
		if (i.currentStyle !== i.fillStyle) {
			Switch.native.canvasSetSourceRgba(i.ctx, ...i.fillStyle);
		}
		Switch.native.canvasFillRect(i.ctx, x, y, w, h);
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
			this[INTERNAL_SYMBOL].ctx,
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
			this[INTERNAL_SYMBOL].ctx,
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

function rgbaToString(rgba: RGBA) {
	if (rgba[3] < 1) {
		return `rgba(${rgba.join(', ')})`;
	}
	return `#${rgba
		.slice(0, -1)
		.map((v) => v.toString(16).padStart(2, '0'))
		.join('')}`;
}
