export interface DOMRectInit {
	height?: number;
	width?: number;
	x?: number;
	y?: number;
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMRectReadOnly) */
export class DOMRectReadOnly {
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMRectReadOnly/bottom) */
	//readonly bottom: number;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMRectReadOnly/height) */
	readonly height: number;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMRectReadOnly/left) */
	//readonly left: number;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMRectReadOnly/right) */
	//readonly right: number;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMRectReadOnly/top) */
	//readonly top: number;
	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMRectReadOnly/width) */
	readonly width: number;
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

	toJSON() {
		return this;
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMRectReadOnly/fromRect_static) */
	static fromRect(other: DOMRectInit = {}): DOMRectReadOnly {
		return new DOMRectReadOnly(other.x, other.y, other.width, other.height);
	}
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMRect) */
export class DOMRect extends DOMRectReadOnly {
	height: number;
	width: number;
	x: number;
	y: number;

	constructor(x?: number, y?: number, width?: number, height?: number) {
		super();
		this.x = x ?? 0;
		this.y = y ?? 0;
		this.width = width ?? 0;
		this.height = height ?? 0;
	}

	static fromRect(other: DOMRectInit = {}): DOMRect {
		return new DOMRect(other.x, other.y, other.width, other.height);
	}
}
