import { def } from './utils';

export interface DOMPointInit {
	w?: number;
	x?: number;
	y?: number;
	z?: number;
}

/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMPointReadOnly) */
export class DOMPointReadOnly implements globalThis.DOMPointReadOnly {
	constructor(x?: number, y?: number, z?: number, w?: number) {
		this.x = x || 0;
		this.y = y || 0;
		this.z = z || 0;
		this.w = w || 1;
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMPointReadOnly/fromPoint) */
	static fromPoint({ x, y, z, w }: DOMPointInit = {}): DOMPointReadOnly {
		return new DOMPointReadOnly(x, y, z, w);
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMPointReadOnly/w) */
	readonly w: number;

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMPointReadOnly/x) */
	readonly x: number;

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMPointReadOnly/y) */
	readonly y: number;

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMPointReadOnly/z) */
	readonly z: number;

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMPointReadOnly/matrixTransform) */
	matrixTransform(matrix?: DOMMatrixInit): DOMPoint {
		throw new Error('Method not implemented.');
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMPointReadOnly/toJSON) */
	toJSON() {
		return { x: this.x, y: this.y, z: this.z, w: this.w };
	}
}
def(DOMPointReadOnly);

/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMPoint) */
export class DOMPoint extends DOMPointReadOnly implements globalThis.DOMPoint {
	constructor(x?: number, y?: number, z?: number, w?: number) {
		super();
		this.x = x || 0;
		this.y = y || 0;
		this.z = z || 0;
		this.w = w || 1;
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMPoint/fromPoint) */
	static fromPoint({ x, y, z, w }: DOMPointInit = {}): DOMPoint {
		return new DOMPoint(x, y, z, w);
	}

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMPoint/w) */
	w: number;

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMPoint/x) */
	x: number;

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMPoint/y) */
	y: number;

	/** [MDN Reference](https://developer.mozilla.org/docs/Web/API/DOMPoint/z) */
	z: number;
}
def(DOMPoint);
