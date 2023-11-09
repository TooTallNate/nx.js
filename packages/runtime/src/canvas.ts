import toPx = require('to-px/index.js');
import colorRgba = require('color-rgba');
import parseCssFont from 'parse-css-font';
import { INTERNAL_SYMBOL } from './internal';
import { Image } from './image';
import { ImageData } from './canvas/image-data';
import { Path2D, applyPath } from './canvas/path2d';
import { addSystemFont, findFont, fontFaceInternal } from './polyfills/font';
import type { CanvasRenderingContext2DState, ImageOpaque } from './switch';
import type { SwitchClass } from './switch';
import type { DOMPointInit } from './dompoint';

declare const Switch: SwitchClass;

export type CanvasImageSource = Image | Canvas;

type RGBA = [number, number, number, number];

const contexts = new WeakMap<Canvas, CanvasRenderingContext2D>();

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
		let ctx = contexts.get(this);
		if (!ctx) {
			ctx = new CanvasRenderingContext2D(this);
			contexts.set(this, ctx);
		}
		return ctx;
	}
}

interface CanvasRenderingContext2DInternal {
	ctx: CanvasRenderingContext2DState;
	fillStyle: RGBA;
	strokeStyle: RGBA;
	currentStyle?: RGBA;
	font: string;
}

const ctxInternalMap = new WeakMap<
	CanvasRenderingContext2D,
	CanvasRenderingContext2DInternal
>();
function internal(ctx: CanvasRenderingContext2D) {
	const i = ctxInternalMap.get(ctx);
	if (!i) {
		throw new Error(
			`Failed to get CanvasRenderingContext2D internal state`
		);
	}
	return i;
}
export { internal as ctxInternal };

export class CanvasRenderingContext2D {
	canvas: Canvas;
	direction: CanvasDirection;
	fontKerning: CanvasFontKerning;
	textAlign: CanvasTextAlign;
	textBaseline: CanvasTextBaseline;
	globalCompositeOperation: GlobalCompositeOperation;
	filter: string;
	imageSmoothingEnabled: boolean;
	imageSmoothingQuality: ImageSmoothingQuality;
	shadowBlur: number;
	shadowColor: string;
	shadowOffsetX: number;
	shadowOffsetY: number;

	constructor(canvas: Canvas) {
		const { width: w, height: h } = canvas;
		this.canvas = canvas;
		ctxInternalMap.set(this, {
			ctx: Switch.native.canvasNewContext(w, h),
			strokeStyle: [0, 0, 0, 1],
			fillStyle: [0, 0, 0, 1],
			font: ''
		});
		this.font = '10px system-ui';

		// TODO: implement
		this.direction = 'ltr';
		this.fontKerning = 'auto';
		this.textAlign = 'left';
		this.textBaseline = 'alphabetic';
		this.globalCompositeOperation = 'source-over';
		this.filter = 'none';
		this.imageSmoothingEnabled = true;
		this.imageSmoothingQuality = 'high';
		this.shadowBlur = 0;
		this.shadowColor = 'rgba(0, 0, 0, 0)';
		this.shadowOffsetX = 0;
		this.shadowOffsetY = 0;
	}

	getContextAttributes(): CanvasRenderingContext2DSettings {
		throw new Error('Method not implemented.');
	}

	drawImage(image: CanvasImageSource, dx: number, dy: number): void;
	drawImage(
		image: CanvasImageSource,
		dx: number,
		dy: number,
		dw: number,
		dh: number
	): void;
	drawImage(
		image: CanvasImageSource,
		sx: number,
		sy: number,
		sw: number,
		sh: number,
		dx: number,
		dy: number,
		dw: number,
		dh: number
	): void;
	drawImage(
		image: CanvasImageSource,
		dxOrSx: number,
		dyOrSy: number,
		dwOrSw?: number,
		dhOrSh?: number,
		dx?: number,
		dy?: number,
		dw?: number,
		dh?: number
	): void {
		let sx = 0;
		let sy = 0;
		let sw: number;
		let sh: number;
		let imageWidth: number;
		let imageHeight: number;
		let opaque: CanvasRenderingContext2DState | ImageOpaque;
		let isCanvas = false;
		if (image instanceof Image) {
			const o = image[INTERNAL_SYMBOL].opaque;
			if (!o) {
				throw new Error('Image not loaded');
			}
			opaque = o;
			imageWidth = sw = image.naturalWidth;
			imageHeight = sh = image.naturalHeight;
		} else if (image instanceof Canvas) {
			const ctx = contexts.get(image);
			if (!ctx) {
				throw new Error('Failed to get Canvas context');
			}
			opaque = internal(ctx).ctx;
			imageWidth = sw = image.width;
			imageHeight = sh = image.height;
			isCanvas = true;
		} else {
			throw new Error('Image type not supported');
		}

		if (typeof dwOrSw === 'number' && typeof dhOrSh === 'number') {
			if (
				typeof dx === 'number' &&
				typeof dy === 'number' &&
				typeof dw === 'number' &&
				typeof dh === 'number'
			) {
				// img, sx, sy, sw, sh, dx, dy, dw, dh
				sx = dxOrSx;
				sy = dyOrSy;
				sw = dwOrSw;
				sh = dhOrSh;
			} else {
				// img, dx, dy, dw, dh
				dx = dxOrSx;
				dy = dyOrSy;
				dw = dwOrSw;
				dh = dhOrSh;
			}
		} else {
			// img, dx, dy
			dx = dxOrSx;
			dy = dyOrSy;
			dw = sw;
			dh = sh;
		}

		Switch.native.canvasDrawImage(
			internal(this).ctx,
			opaque,
			imageWidth,
			imageHeight,
			sx,
			sy,
			sw,
			sh,
			dx,
			dy,
			dw,
			dh,
			isCanvas
		);
	}

	drawFocusIfNeeded(element: Element): void;
	drawFocusIfNeeded(path: Path2D, element: Element): void;
	drawFocusIfNeeded(path: unknown, element?: unknown): void {
		throw new Error('Method not implemented.');
	}

	restore(): void {
		Switch.native.canvasRestore(internal(this).ctx);
	}

	save(): void {
		Switch.native.canvasSave(internal(this).ctx);
	}

	rotate(angle: number): void {
		Switch.native.canvasRotate(internal(this).ctx, angle);
	}

	scale(x: number, y: number): void {
		Switch.native.canvasScale(internal(this).ctx, x, y);
	}

	getTransform(): DOMMatrix {
		return new DOMMatrix(
			Switch.native.canvasGetTransform(internal(this).ctx)
		);
	}

	resetTransform(): void {
		Switch.native.canvasResetTransform(internal(this).ctx);
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
		Switch.native.canvasTransform(internal(this).ctx, a, b, c, d, e, f);
	}

	translate(x: number, y: number): void {
		Switch.native.canvasTranslate(internal(this).ctx, x, y);
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
			internal(this).ctx,
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
		Switch.native.canvasArcTo(internal(this).ctx, x1, y1, x2, y2, radius);
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
			internal(this).ctx,
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
			internal(this).ctx,
			cpx,
			cpy,
			x,
			y
		);
	}

	closePath(): void {
		Switch.native.canvasClosePath(internal(this).ctx);
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
			internal(this).ctx,
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
		Switch.native.canvasLineTo(internal(this).ctx, x, y);
	}

	moveTo(x: number, y: number): void {
		Switch.native.canvasMoveTo(internal(this).ctx, x, y);
	}

	rect(x: number, y: number, w: number, h: number): void {
		Switch.native.canvasRect(internal(this).ctx, x, y, w, h);
	}

	/**
	 * Implementation from https://github.com/nilzona/path2d-polyfill
	 *
	 * @note Currently does not support negative width / height values.
	 */
	roundRect(
		x: number,
		y: number,
		width: number,
		height: number,
		radii: number | DOMPointInit | Iterable<number | DOMPointInit> = 0
	): void {
		const r: number[] = (
			typeof radii === 'number' || isDomPointInit(radii)
				? [radii]
				: Array.from(radii)
		).map<number>((v) => {
			if (typeof v !== 'number') {
				// DOMPoint
				v = Math.sqrt(
					(v.x || 0) * (v.x || 0) + (v.y || 0) * (v.y || 0)
				);
			}
			if (v < 0) {
				throw new RangeError(`Radius value ${v} is negative.`);
			}
			return v;
		});

		// check for range error
		if (r.length === 0 || r.length > 4) {
			throw new RangeError(
				`${r.length} radii provided. Between one and four radii are necessary.`
			);
		}

		if (r.length === 1 && r[0] === 0) {
			return this.rect(x, y, width, height);
		}

		// set the corners
		// tl = top left radius
		// tr = top right radius
		// br = bottom right radius
		// bl = bottom left radius
		const minRadius = Math.min(width, height) / 2;
		let tr, br, bl;
		const tl = (tr = br = bl = Math.min(minRadius, r[0]));
		if (r.length === 2) {
			tr = bl = Math.min(minRadius, r[1]);
		}
		if (r.length === 3) {
			tr = bl = Math.min(minRadius, r[1]);
			br = Math.min(minRadius, r[2]);
		}
		if (r.length === 4) {
			tr = Math.min(minRadius, r[1]);
			br = Math.min(minRadius, r[2]);
			bl = Math.min(minRadius, r[3]);
		}

		// begin with closing current path
		this.closePath();

		// draw the rounded rectangle
		this.moveTo(x, y + height - bl);
		this.arcTo(x, y, x + tl, y, tl);
		this.arcTo(x + width, y, x + width, y + tr, tr);
		this.arcTo(x + width, y + height, x + width - br, y + height, br);
		this.arcTo(x, y + height, x, y + height - bl, bl);

		// move to rects control point for further path drawing
		this.moveTo(x, y);
	}

	beginPath(): void {
		Switch.native.canvasBeginPath(internal(this).ctx);
	}

	clip(fillRule?: CanvasFillRule): void;
	clip(path: Path2D, fillRule?: CanvasFillRule): void;
	clip(
		pathOrFillRule?: CanvasFillRule | Path2D,
		fillRule?: CanvasFillRule
	): void {
		let path: Path2D | undefined;
		if (typeof pathOrFillRule === 'object') {
			path = pathOrFillRule;
		} else {
			fillRule = pathOrFillRule;
		}
		Switch.native.canvasClip(internal(this).ctx, fillRule);
	}

	fill(fillRule?: CanvasFillRule): void;
	fill(path: Path2D, fillRule?: CanvasFillRule): void;
	fill(
		fillRuleOrPath?: CanvasFillRule | Path2D,
		fillRule?: CanvasFillRule
	): void {
		let path: Path2D | undefined;
		if (typeof fillRuleOrPath === 'string') {
			fillRule = fillRuleOrPath;
		} else if (fillRuleOrPath) {
			path = fillRuleOrPath;
		}
		if (path) {
			applyPath(this, path);
		}
		const i = internal(this);
		if (i.currentStyle !== i.fillStyle) {
			Switch.native.canvasSetSourceRgba(i.ctx, ...i.fillStyle);
			i.currentStyle = i.fillStyle;
		}
		Switch.native.canvasFill(i.ctx);
	}

	isPointInPath(x: number, y: number, fillRule?: CanvasFillRule): boolean;
	isPointInPath(
		path: Path2D,
		x: number,
		y: number,
		fillRule?: CanvasFillRule
	): boolean;
	isPointInPath(
		pathOrX: Path2D | number,
		xOrY: number,
		yOrFillRule?: number | CanvasFillRule,
		fillRule?: CanvasFillRule
	): boolean {
		let path: Path2D | undefined;
		let x: number;
		let y: number;
		if (typeof pathOrX === 'object') {
			path = pathOrX;
			x = xOrY;
			if (typeof yOrFillRule !== 'number') {
				throw new TypeError('Expected third argument to be "number"');
			}
			y = yOrFillRule;
		} else {
			x = pathOrX;
			y = xOrY;
			if (typeof yOrFillRule === 'string') {
				fillRule = yOrFillRule;
			}
		}
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
		const i = internal(this);
		if (i.currentStyle !== i.strokeStyle) {
			Switch.native.canvasSetSourceRgba(i.ctx, ...i.strokeStyle);
			i.currentStyle = i.strokeStyle;
		}
		Switch.native.canvasStroke(i.ctx);
	}

	getLineDash(): number[] {
		return Switch.native.canvasGetLineDash(internal(this).ctx);
	}

	setLineDash(segments: Iterable<number>): void {
		Switch.native.canvasSetLineDash(
			internal(this).ctx,
			Array.from(segments)
		);
	}

	get globalAlpha() {
		return Switch.native.canvasGetGlobalAlpha(internal(this).ctx);
	}

	set globalAlpha(v: number) {
		Switch.native.canvasSetGlobalAlpha(internal(this).ctx, v);
	}

	get lineDashOffset() {
		return Switch.native.canvasGetLineDashOffset(internal(this).ctx);
	}

	set lineDashOffset(v: number) {
		Switch.native.canvasSetLineDashOffset(internal(this).ctx, v);
	}

	get lineWidth() {
		return Switch.native.canvasGetLineWidth(internal(this).ctx);
	}

	set lineWidth(v: number) {
		Switch.native.canvasSetLineWidth(internal(this).ctx, v);
	}

	get lineJoin() {
		return Switch.native.canvasGetLineJoin(internal(this).ctx);
	}

	set lineJoin(v: CanvasLineJoin) {
		Switch.native.canvasSetLineJoin(internal(this).ctx, v);
	}

	get lineCap() {
		return Switch.native.canvasGetLineCap(internal(this).ctx);
	}

	set lineCap(v: CanvasLineCap) {
		Switch.native.canvasSetLineCap(internal(this).ctx, v);
	}

	get miterLimit() {
		return Switch.native.canvasGetMiterLimit(internal(this).ctx);
	}

	set miterLimit(v: number) {
		Switch.native.canvasSetMiterLimit(internal(this).ctx, v);
	}

	get fillStyle(): string | CanvasGradient | CanvasPattern {
		return rgbaToString(internal(this).fillStyle);
	}

	set fillStyle(v: string | CanvasGradient | CanvasPattern) {
		if (typeof v === 'string') {
			const parsed = colorRgba(v);
			if (!parsed || parsed.length !== 4) {
				return;
			}
			internal(this).fillStyle = parsed;
		} else {
			throw new Error('CanvasGradient/CanvasPattern not implemented.');
		}
	}

	get strokeStyle(): string | CanvasGradient | CanvasPattern {
		return rgbaToString(internal(this).strokeStyle);
	}

	set strokeStyle(v: string | CanvasGradient | CanvasPattern) {
		if (typeof v === 'string') {
			const parsed = colorRgba(v);
			if (!parsed || parsed.length !== 4) {
				return;
			}
			internal(this).strokeStyle = parsed;
		} else {
			throw new Error('CanvasGradient/CanvasPattern not implemented.');
		}
	}

	get font(): string {
		return internal(this).font;
	}

	set font(v: string) {
		if (!v) return;
		const i = internal(this);
		if (i.font === v) return;

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
		let font = findFont(fonts, parsed);
		if (!font) {
			if (parsed.family.includes('system-ui')) {
				font = addSystemFont(fonts);
			} else {
				return;
			}
		}
		i.font = v;
		native.canvasSetFont(
			i.ctx,
			fontFaceInternal.get(font)!.fontFace,
			px
		);
	}

	fillText(
		text: string,
		x: number,
		y: number,
		maxWidth?: number | undefined
	): void {
		const i = internal(this);
		if (i.currentStyle !== i.fillStyle) {
			Switch.native.canvasSetSourceRgba(i.ctx, ...i.fillStyle);
			i.currentStyle = i.fillStyle;
		}
		Switch.native.canvasFillText(i.ctx, text, x, y, maxWidth);
	}

	measureText(text: string): TextMetrics {
		return Switch.native.canvasMeasureText(internal(this).ctx, text);
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
		this.save();
		this.fillStyle = 'rgba(0,0,0,0)';
		this.fillRect(x, y, w, h);
		this.restore();
	}

	fillRect(x: number, y: number, w: number, h: number): void {
		const i = internal(this);
		if (i.currentStyle !== i.fillStyle) {
			Switch.native.canvasSetSourceRgba(i.ctx, ...i.fillStyle);
			i.currentStyle = i.strokeStyle;
		}
		Switch.native.canvasFillRect(i.ctx, x, y, w, h);
	}

	strokeRect(x: number, y: number, w: number, h: number): void {
		const i = internal(this);
		if (i.currentStyle !== i.strokeStyle) {
			Switch.native.canvasSetSourceRgba(i.ctx, ...i.fillStyle);
			i.currentStyle = i.strokeStyle;
		}
		Switch.native.canvasStrokeRect(i.ctx, x, y, w, h);
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
			internal(this).ctx,
			sx,
			sy,
			sw,
			sh
		);
		const data = new Uint8ClampedArray(buffer);
		return new ImageData(data, sw, sh, settings);
	}

	putImageData(imagedata: ImageData, dx: number, dy: number): void;
	putImageData(
		imagedata: ImageData,
		dx: number,
		dy: number,
		dirtyX: number,
		dirtyY: number,
		dirtyWidth: number,
		dirtyHeight: number
	): void;
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
			internal(this).ctx,
			imagedata.data.buffer,
			imagedata.width,
			imagedata.height,
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

function isDomPointInit(v: any): v is DOMPointInit {
	return v && typeof v.x === 'number' && typeof v.y === 'number';
}
