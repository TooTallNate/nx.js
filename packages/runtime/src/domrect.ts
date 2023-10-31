import { def } from './utils';

export interface DOMRectInit {
	height?: number;
	width?: number;
	x?: number;
	y?: number;
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMRectReadOnly) */
export class DOMRectReadOnly {
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMRectReadOnly/width) */
	readonly width: number;

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMRectReadOnly/height) */
	readonly height: number;

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMRectReadOnly/x) */
	readonly x: number;

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMRectReadOnly/y) */
	readonly y: number;

	constructor(x?: number, y?: number, width?: number, height?: number) {
		this.x = x ?? 0;
		this.y = y ?? 0;
		this.width = width ?? 0;
		this.height = height ?? 0;
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMRectReadOnly/bottom) */
	get bottom() {
		return this.y + this.height;
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMRectReadOnly/left) */
	get left() {
		return this.x;
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMRectReadOnly/right) */
	get right() {
		return this.x + this.width;
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMRectReadOnly/top) */
	get top() {
		return this.y;
	}

	toJSON() {
		return {
			x: this.x,
			y: this.y,
			width: this.width,
			height: this.height,
			top: this.top,
			right: this.right,
			bottom: this.bottom,
			left: this.left,
		};
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMRectReadOnly/fromRect_static) */
	static fromRect(o: DOMRectInit = {}): DOMRectReadOnly {
		return new DOMRectReadOnly(o.x, o.y, o.width, o.height);
	}
}
def('DOMRectReadOnly', DOMRectReadOnly);

/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMRect) */
export class DOMRect extends DOMRectReadOnly {
	width: number;
	height: number;
	x: number;
	y: number;

	constructor(x?: number, y?: number, width?: number, height?: number) {
		super();
		this.x = x ?? 0;
		this.y = y ?? 0;
		this.width = width ?? 0;
		this.height = height ?? 0;
	}

	static fromRect(o: DOMRectInit = {}): DOMRect {
		return new DOMRect(o.x, o.y, o.width, o.height);
	}
}
def('DOMRect', DOMRect);
