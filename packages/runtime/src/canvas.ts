import toPx = require('to-px/index.js');
import colorRgba = require('color-rgba');
import parseCssFont from 'parse-css-font';
import { INTERNAL_SYMBOL } from './internal';
import { Image } from './image';
import { Path2D, applyPath } from './canvas/path2d';
import { addSystemFont, findFont, fontFaceInternal } from './polyfills/font';
import type { CanvasRenderingContext2DState, ImageOpaque } from './switch';
import type { SwitchClass } from './switch';

declare const Switch: SwitchClass;

type RGBA = [number, number, number, number];

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
			font: '',
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

	clip(fillRule?: CanvasFillRule): void;
	clip(path: Path2D, fillRule?: CanvasFillRule): void;
	clip(
		pathOrFillRule?: CanvasFillRule | Path2D,
		fillRule?: CanvasFillRule
	): void {
		//let path: Path2D | undefined;
		if (typeof pathOrFillRule === 'object') {
			//path = pathOrFillRule;
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
		//let path: Path2D | undefined;
		//let x: number;
		//let y: number;
		//if (typeof pathOrX === 'object') {
		//	path = pathOrX;
		//	x = xOrY;
		//	if (typeof yOrFillRule !== 'number') {
		//		throw new TypeError('Expected third argument to be "number"');
		//	}
		//	y = yOrFillRule;
		//} else {
		//	x = pathOrX;
		//	y = xOrY;
		//	if (typeof yOrFillRule === 'string') {
		//		fillRule = yOrFillRule;
		//	}
		//}
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
		native.canvasSetFont(i.ctx, fontFaceInternal.get(font)!.fontFace, px);
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
