// Spec: https://drafts.fxtf.org/geometry/#DOMMatrix
// Ref: https://github.com/Automattic/node-canvas/blob/master/lib/DOMMatrix.js
import { $ } from './$';
import { def, proto, stub } from './utils';
import { DOMPoint, type DOMPointInit } from './dompoint';

const DEGREE_PER_RAD = 180 / Math.PI;

export interface DOMMatrix2DInit {
	a?: number;
	b?: number;
	c?: number;
	d?: number;
	e?: number;
	f?: number;
	m11?: number;
	m12?: number;
	m21?: number;
	m22?: number;
	m41?: number;
	m42?: number;
}

export interface DOMMatrixInit extends DOMMatrix2DInit {
	is2D?: boolean;
	m13?: number;
	m14?: number;
	m23?: number;
	m24?: number;
	m31?: number;
	m32?: number;
	m33?: number;
	m34?: number;
	m43?: number;
	m44?: number;
}

const arr = (m: DOMMatrixReadOnly) => [
	m.m11,
	m.m12,
	m.m13,
	m.m14,
	m.m21,
	m.m22,
	m.m23,
	m.m24,
	m.m31,
	m.m32,
	m.m33,
	m.m34,
	m.m41,
	m.m42,
	m.m43,
	m.m44,
];

let FLIP_X_MATRIX: DOMMatrixReadOnly | undefined;
let FLIP_Y_MATRIX: DOMMatrixReadOnly | undefined;

export class DOMMatrixReadOnly implements globalThis.DOMMatrixReadOnly {
	declare readonly a: number;
	declare readonly b: number;
	declare readonly c: number;
	declare readonly d: number;
	declare readonly e: number;
	declare readonly f: number;
	declare readonly m11: number;
	declare readonly m12: number;
	declare readonly m13: number;
	declare readonly m14: number;
	declare readonly m21: number;
	declare readonly m22: number;
	declare readonly m23: number;
	declare readonly m24: number;
	declare readonly m31: number;
	declare readonly m32: number;
	declare readonly m33: number;
	declare readonly m34: number;
	declare readonly m41: number;
	declare readonly m42: number;
	declare readonly m43: number;
	declare readonly m44: number;
	declare readonly is2D: boolean;
	declare readonly isIdentity: boolean;

	constructor(init?: string | number[]) {
		if (typeof init === 'string') {
			throw new Error('String param not implemented.');
		}
		return proto($.dommatrixNew(init), new.target);
	}

	flipX(): DOMMatrix {
		if (!FLIP_X_MATRIX)
			FLIP_X_MATRIX = new DOMMatrixReadOnly([-1, 0, 0, 1, 0, 0]);
		return DOMMatrix.fromMatrix(this).multiplySelf(FLIP_X_MATRIX);
	}

	flipY(): DOMMatrix {
		if (!FLIP_Y_MATRIX)
			FLIP_Y_MATRIX = new DOMMatrixReadOnly([1, 0, 0, -1, 0, 0]);
		return DOMMatrix.fromMatrix(this).multiplySelf(FLIP_Y_MATRIX);
	}

	inverse(): DOMMatrix {
		return DOMMatrix.fromMatrix(this).invertSelf();
	}

	multiply(other?: DOMMatrixInit): DOMMatrix {
		return DOMMatrix.fromMatrix(this).multiplySelf(other);
	}

	rotate(rotX?: number, rotY?: number, rotZ?: number): DOMMatrix {
		return DOMMatrix.fromMatrix(this).rotateSelf(rotX, rotY, rotZ);
	}

	rotateAxisAngle(
		x?: number,
		y?: number,
		z?: number,
		angle?: number
	): DOMMatrix {
		return DOMMatrix.fromMatrix(this).rotateAxisAngleSelf(x, y, z, angle);
	}

	rotateFromVector(x?: number, y?: number): DOMMatrix {
		return DOMMatrix.fromMatrix(this).rotateFromVectorSelf(x, y);
	}

	scale(
		scaleX?: number,
		scaleY?: number,
		scaleZ?: number,
		originX?: number,
		originY?: number,
		originZ?: number
	): DOMMatrix {
		return DOMMatrix.fromMatrix(this).scaleSelf(
			scaleX,
			scaleY,
			scaleZ,
			originX,
			originY,
			originZ
		);
	}

	scale3d(
		scale?: number,
		originX?: number,
		originY?: number,
		originZ?: number
	): DOMMatrix {
		return DOMMatrix.fromMatrix(this).scale3dSelf(
			scale,
			originX,
			originY,
			originZ
		);
	}

	scaleNonUniform(scaleX?: number, scaleY?: number): DOMMatrix {
		return DOMMatrix.fromMatrix(this).scaleSelf(scaleX, scaleY, 1, 0, 0, 0);
	}

	skewX(sx?: number): DOMMatrix {
		return DOMMatrix.fromMatrix(this).skewXSelf(sx);
	}

	skewY(sy?: number): DOMMatrix {
		return DOMMatrix.fromMatrix(this).skewYSelf(sy);
	}

	toFloat32Array(): Float32Array {
		return new Float32Array(arr(this));
	}

	toFloat64Array(): Float64Array {
		return new Float64Array(arr(this));
	}

	transformPoint(point: DOMPointInit = {}): DOMPoint {
		return proto($.dommatrixTransformPoint(this, point), DOMPoint);
	}

	translate(tx?: number, ty?: number, tz?: number): DOMMatrix {
		return DOMMatrix.fromMatrix(this).translateSelf(tx, ty, tz);
	}

	toJSON() {
		return {
			a: this.a,
			b: this.b,
			c: this.c,
			d: this.d,
			e: this.e,
			f: this.f,
			is2D: this.is2D,
			isIdentity: this.isIdentity,
			m11: this.m11,
			m12: this.m12,
			m13: this.m13,
			m14: this.m14,
			m21: this.m21,
			m22: this.m22,
			m23: this.m23,
			m24: this.m24,
			m31: this.m31,
			m32: this.m32,
			m33: this.m33,
			m34: this.m34,
			m41: this.m41,
			m42: this.m42,
			m43: this.m43,
			m44: this.m44,
		};
	}

	toString(): string {
		return this.is2D
			? `matrix(${this.a}, ${this.b}, ${this.c}, ${this.d}, ${this.e}, ${this.f})`
			: `matrix3d(${arr(this).join(', ')})`;
	}

	static fromFloat32Array(init: Float32Array): DOMMatrixReadOnly {
		return new DOMMatrixReadOnly([...init]);
	}

	static fromFloat64Array(init: Float64Array): DOMMatrixReadOnly {
		return new DOMMatrixReadOnly([...init]);
	}

	static fromMatrix(init?: DOMMatrixInit): DOMMatrixReadOnly {
		return proto($.dommatrixFromMatrix(init), DOMMatrixReadOnly);
	}
}
$.dommatrixROInitClass(DOMMatrixReadOnly);
def(DOMMatrixReadOnly);

export class DOMMatrix
	extends DOMMatrixReadOnly
	implements globalThis.DOMMatrix
{
	declare a: number;
	declare b: number;
	declare c: number;
	declare d: number;
	declare e: number;
	declare f: number;
	declare m11: number;
	declare m12: number;
	declare m13: number;
	declare m14: number;
	declare m21: number;
	declare m22: number;
	declare m23: number;
	declare m24: number;
	declare m31: number;
	declare m32: number;
	declare m33: number;
	declare m34: number;
	declare m41: number;
	declare m42: number;
	declare m43: number;
	declare m44: number;

	invertSelf(): DOMMatrix {
		throw new Error('Method not implemented.');
	}
	multiplySelf(other?: DOMMatrixInit): DOMMatrix {
		throw new Error('Method not implemented.');
	}
	preMultiplySelf(other?: DOMMatrixInit): DOMMatrix {
		throw new Error('Method not implemented.');
	}
	rotateAxisAngleSelf(
		x?: number,
		y?: number,
		z?: number,
		angle?: number
	): DOMMatrix {
		throw new Error('Method not implemented.');
	}

	rotateFromVectorSelf(x = 0, y = 0): DOMMatrix {
		const theta =
			x === 0 && y === 0 ? 0 : Math.atan2(y, x) * DEGREE_PER_RAD;
		return this.rotateSelf(theta);
	}

	rotateSelf(rotX?: number, rotY?: number, rotZ?: number): DOMMatrix {
		stub();
	}

	scale3dSelf(
		scale?: number,
		originX?: number,
		originY?: number,
		originZ?: number
	): DOMMatrix {
		return this.scaleSelf(scale, scale, scale, originX, originY, originZ);
	}

	scaleSelf(
		scaleX?: number,
		scaleY?: number,
		scaleZ?: number,
		originX?: number,
		originY?: number,
		originZ?: number
	): DOMMatrix {
		stub();
	}

	setMatrixValue(transformList: string): DOMMatrix {
		throw new Error('Method not implemented.');
	}

	skewXSelf(sx?: number): DOMMatrix {
		stub();
	}

	skewYSelf(sy?: number): DOMMatrix {
		stub();
	}

	translateSelf(tx?: number, ty?: number, tz?: number): DOMMatrix {
		stub();
	}

	static fromFloat32Array(init: Float32Array): DOMMatrix {
		return new DOMMatrix([...init]);
	}

	static fromFloat64Array(init: Float64Array): DOMMatrix {
		return new DOMMatrix([...init]);
	}

	static fromMatrix(init?: DOMMatrixInit): DOMMatrix {
		return proto($.dommatrixFromMatrix(init), DOMMatrix);
	}
}
$.dommatrixInitClass(DOMMatrix);
def(DOMMatrix);
