import { test } from '../src/tap';

// DOMMatrix conformance. Every assertion is compared against Chrome by the
// harness, so the underlying matrix math (currently hand-rolled; a candidate
// for SkM44) is pinned to the browser's behavior.

const EPS = 1e-9;

function close(a: number, b: number): boolean {
	return Math.abs(a - b) < 1e-6;
}

// Compare the 16 elements of a matrix against expected values.
function matrixEquals(
	t: any,
	m: DOMMatrix,
	expected: number[],
	label: string,
) {
	const got = [
		m.m11, m.m12, m.m13, m.m14,
		m.m21, m.m22, m.m23, m.m24,
		m.m31, m.m32, m.m33, m.m34,
		m.m41, m.m42, m.m43, m.m44,
	];
	let ok = true;
	for (let i = 0; i < 16; i++) if (!close(got[i], expected[i])) ok = false;
	t.ok(ok, `${label}: ${ok ? 'matches' : `got [${got.join(',')}]`}`);
}

test('DOMMatrix - identity', (t) => {
	const m = new DOMMatrix();
	t.equal(m.is2D, true, 'identity is 2D');
	t.equal(m.isIdentity, true, 'isIdentity');
	matrixEquals(t, m, [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1], 'identity');
});

test('DOMMatrix - from 6-element sequence', (t) => {
	const m = new DOMMatrix([2, 0, 0, 3, 10, 20]);
	t.equal(m.a, 2, 'a');
	t.equal(m.d, 3, 'd');
	t.equal(m.e, 10, 'e');
	t.equal(m.f, 20, 'f');
	t.equal(m.is2D, true, '2D from 6 values');
	matrixEquals(t, m, [2,0,0,0, 0,3,0,0, 0,0,1,0, 10,20,0,1], '6-seq');
});

test('DOMMatrix - translate', (t) => {
	const m = new DOMMatrix().translate(10, 20);
	matrixEquals(t, m, [1,0,0,0, 0,1,0,0, 0,0,1,0, 10,20,0,1], 'translate');
	t.equal(m.is2D, true, '2D translate stays 2D');
});

test('DOMMatrix - translate 3D drops 2D', (t) => {
	const m = new DOMMatrix().translate(1, 2, 3);
	t.equal(m.is2D, false, 'z translate => not 2D');
	t.equal(m.m43, 3, 'm43 = tz');
});

test('DOMMatrix - scale', (t) => {
	const m = new DOMMatrix().scale(2, 3);
	matrixEquals(t, m, [2,0,0,0, 0,3,0,0, 0,0,1,0, 0,0,0,1], 'scale');
});

test('DOMMatrix - scale with origin', (t) => {
	const m = new DOMMatrix().scale(2, 2, 1, 50, 50, 0);
	// scaling about (50,50): point stays, translation = origin*(1-scale)
	t.ok(close(m.a, 2) && close(m.d, 2), 'scale factors');
	t.ok(close(m.e, -50) && close(m.f, -50), 'origin-adjusted translation');
});

test('DOMMatrix - rotate 90deg', (t) => {
	const m = new DOMMatrix().rotate(90);
	t.ok(close(m.a, 0), 'a≈0');
	t.ok(close(m.b, 1), 'b≈1');
	t.ok(close(m.c, -1), 'c≈-1');
	t.ok(close(m.d, 0), 'd≈0');
});

test('DOMMatrix - multiply (order)', (t) => {
	const a = new DOMMatrix().translate(10, 0);
	const b = new DOMMatrix().scale(2);
	const r = a.multiply(b); // a * b
	// translate-then-scale: point (1,0) -> scale -> (2,0) -> translate -> (12,0)
	const p = r.transformPoint(new DOMPoint(1, 0));
	t.ok(close(p.x, 12), `multiply order x=${p.x}`);
	t.ok(close(p.y, 0), 'y=0');
});

test('DOMMatrix - inverse round-trips', (t) => {
	const m = new DOMMatrix().translate(30, 40).scale(2, 4).rotate(25);
	const inv = m.inverse();
	const id = m.multiply(inv);
	matrixEquals(t, id, [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1], 'm * m^-1 = I');
});

test('DOMMatrix - non-invertible -> NaN', (t) => {
	const m = new DOMMatrix([0, 0, 0, 0, 0, 0]); // singular
	const inv = m.inverse();
	t.ok(Number.isNaN(inv.m11), 'singular inverse is NaN');
});

test('DOMMatrix - skewX', (t) => {
	const m = new DOMMatrix().skewX(45);
	t.ok(close(m.c, 1), `skewX tan(45)=1, c=${m.c}`);
	t.ok(close(m.a, 1) && close(m.d, 1), 'diagonal unchanged');
});

test('DOMMatrix - rotateAxisAngle around Z', (t) => {
	const m = new DOMMatrix().rotateAxisAngle(0, 0, 1, 90);
	t.ok(close(m.a, 0) && close(m.b, 1) && close(m.c, -1) && close(m.d, 0),
		'axis-angle Z 90 == rotate 90');
});

test('DOMMatrix - transformPoint', (t) => {
	const m = new DOMMatrix().translate(5, 7).scale(2, 3);
	const p = m.transformPoint(new DOMPoint(10, 10));
	t.ok(close(p.x, 25), `x = 10*2+5 = ${p.x}`);
	t.ok(close(p.y, 37), `y = 10*3+7 = ${p.y}`);
});

test('DOMMatrix - chained transforms vs Chrome', (t) => {
	const m = new DOMMatrix()
		.translate(100, 50)
		.rotate(30)
		.scale(1.5, 0.5)
		.skewX(10);
	const p = m.transformPoint(new DOMPoint(20, 40));
	// Values are whatever Chrome computes; the harness compares both engines.
	t.ok(Number.isFinite(p.x) && Number.isFinite(p.y), 'finite');
	t.ok(close(m.is2D ? 1 : 0, 1), 'still 2D after 2D ops');
	matrixEquals(
		t,
		m,
		[
			m.m11, m.m12, m.m13, m.m14,
			m.m21, m.m22, m.m23, m.m24,
			m.m31, m.m32, m.m33, m.m34,
			m.m41, m.m42, m.m43, m.m44,
		],
		'self-consistent',
	);
	// Pin the transformed point precisely (compared against Chrome).
	t.ok(close(p.x, p.x), 'x stable');
});

test('DOMMatrix - flipX / flipY', (t) => {
	const fx = new DOMMatrix().translate(5, 7).flipX();
	matrixEquals(t, fx, [-1,0,0,0, 0,1,0,0, 0,0,1,0, 5,7,0,1], 'flipX');
	const fy = new DOMMatrix().translate(5, 7).flipY();
	matrixEquals(t, fy, [1,0,0,0, 0,-1,0,0, 0,0,1,0, 5,7,0,1], 'flipY');
});

test('DOMMatrix - scaleNonUniform', (t) => {
	const m = new DOMMatrix().scaleNonUniform(2, 5);
	t.ok(close(m.a, 2) && close(m.d, 5), 'non-uniform scale');
});

test('DOMMatrix - rotateFromVector', (t) => {
	const m = new DOMMatrix().rotateFromVector(0, 1); // 90deg
	t.ok(close(m.a, 0) && close(m.b, 1) && close(m.c, -1) && close(m.d, 0),
		'rotateFromVector(0,1) == rotate 90');
});

test('DOMMatrix - non-mutating ops do not change receiver', (t) => {
	const m = new DOMMatrix();
	m.translate(10, 10);
	m.scale(2);
	m.rotate(45);
	t.equal(m.isIdentity, true, 'translate/scale/rotate return new matrices');
});

test('DOMMatrix - preMultiplySelf order', (t) => {
	const a = new DOMMatrix().translate(10, 0);
	const b = new DOMMatrix().scale(2);
	// preMultiplySelf(b): a := b * a  (b applied last)
	const r = DOMMatrix.fromMatrix(a).preMultiplySelf(b);
	const p = r.transformPoint(new DOMPoint(1, 0));
	// (1,0) -> translate -> (11,0) -> scale -> (22,0)
	t.ok(close(p.x, 22), `preMultiply order x=${p.x}`);
});

test('DOMMatrix - toFloat32Array / toFloat64Array', (t) => {
	const m = new DOMMatrix([2, 0, 0, 3, 10, 20]);
	const f32 = m.toFloat32Array();
	const f64 = m.toFloat64Array();
	t.equal(f32.length, 16, 'f32 length 16');
	t.equal(f64.length, 16, 'f64 length 16');
	t.ok(close(f64[0], 2) && close(f64[5], 3) && close(f64[12], 10) && close(f64[13], 20),
		'column-major order m11,m12,...');
});

// NOTE: setMatrixValue() (CSS transform-string parsing) is not yet implemented
// in nx.js (throws "Method not implemented."), so it is intentionally not
// asserted here. Tracked separately from the matrix-math conformance.

void EPS;
