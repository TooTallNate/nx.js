import { def } from './utils';

export interface DOMPointInit {
	w?: number;
	x?: number;
	y?: number;
	z?: number;
}

export function isDomPointInit(v: any): v is DOMPointInit {
	return v && typeof v.x === 'number' && typeof v.y === 'number';
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
	static fromPoint(other?: DOMPointInit): DOMPointReadOnly {
		return new DOMPointReadOnly(other?.x, other?.y, other?.z, other?.w);
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
def('DOMPointReadOnly', DOMPointReadOnly);

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
	static fromPoint(other?: DOMPointInit): DOMPoint {
		return new DOMPoint(other?.x, other?.y, other?.z, other?.w);
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
def('DOMPoint', DOMPoint);
