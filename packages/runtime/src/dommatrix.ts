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

export class DOMMatrix implements globalThis.DOMMatrix {
	a: number;
	b: number;
	c: number;
	d: number;
	e: number;
	f: number;
	m11: number;
	m12: number;
	m13: number;
	m14: number;
	m21: number;
	m22: number;
	m23: number;
	m24: number;
	m31: number;
	m32: number;
	m33: number;
	m34: number;
	m41: number;
	m42: number;
	m43: number;
	m44: number;
	invertSelf(): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	multiplySelf(
		other?: globalThis.DOMMatrixInit | undefined
	): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	preMultiplySelf(
		other?: globalThis.DOMMatrixInit | undefined
	): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	rotateAxisAngleSelf(
		x?: number | undefined,
		y?: number | undefined,
		z?: number | undefined,
		angle?: number | undefined
	): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	rotateFromVectorSelf(
		x?: number | undefined,
		y?: number | undefined
	): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	rotateSelf(
		rotX?: number | undefined,
		rotY?: number | undefined,
		rotZ?: number | undefined
	): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	scale3dSelf(
		scale?: number | undefined,
		originX?: number | undefined,
		originY?: number | undefined,
		originZ?: number | undefined
	): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	scaleSelf(
		scaleX?: number | undefined,
		scaleY?: number | undefined,
		scaleZ?: number | undefined,
		originX?: number | undefined,
		originY?: number | undefined,
		originZ?: number | undefined
	): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	setMatrixValue(transformList: string): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	skewXSelf(sx?: number | undefined): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	skewYSelf(sy?: number | undefined): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	translateSelf(
		tx?: number | undefined,
		ty?: number | undefined,
		tz?: number | undefined
	): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	is2D: boolean;
	isIdentity: boolean;
	flipX(): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	flipY(): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	inverse(): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	multiply(
		other?: globalThis.DOMMatrixInit | undefined
	): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	rotate(
		rotX?: number | undefined,
		rotY?: number | undefined,
		rotZ?: number | undefined
	): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	rotateAxisAngle(
		x?: number | undefined,
		y?: number | undefined,
		z?: number | undefined,
		angle?: number | undefined
	): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	rotateFromVector(
		x?: number | undefined,
		y?: number | undefined
	): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	scale(
		scaleX?: number | undefined,
		scaleY?: number | undefined,
		scaleZ?: number | undefined,
		originX?: number | undefined,
		originY?: number | undefined,
		originZ?: number | undefined
	): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	scale3d(
		scale?: number | undefined,
		originX?: number | undefined,
		originY?: number | undefined,
		originZ?: number | undefined
	): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	scaleNonUniform(
		scaleX?: number | undefined,
		scaleY?: number | undefined
	): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	skewX(sx?: number | undefined): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	skewY(sy?: number | undefined): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	toFloat32Array(): Float32Array {
		throw new Error('Method not implemented.');
	}
	toFloat64Array(): Float64Array {
		throw new Error('Method not implemented.');
	}
	toJSON() {
		throw new Error('Method not implemented.');
	}
	transformPoint(point?: DOMPointInit | undefined): DOMPoint {
		throw new Error('Method not implemented.');
	}
	translate(
		tx?: number | undefined,
		ty?: number | undefined,
		tz?: number | undefined
	): globalThis.DOMMatrix {
		throw new Error('Method not implemented.');
	}
	toString(): string {
		throw new Error('Method not implemented.');
	}
}
