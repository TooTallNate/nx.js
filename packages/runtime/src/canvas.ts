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

	clearRect(x: number, y: number, w: number, h: number): void {
		this.save();
		this.fillStyle = 'rgba(0,0,0,0)';
		this.fillRect(x, y, w, h);
		this.restore();
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
