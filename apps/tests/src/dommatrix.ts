import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('DOMMatrix');

test('instanceof', () => {
	const d = new DOMMatrix();
	assert.equal(d instanceof DOMMatrix, true);
	assert.equal(d instanceof DOMMatrixReadOnly, true);

	const ro = new DOMMatrixReadOnly();
	assert.equal(ro instanceof DOMMatrix, false);
	assert.equal(ro instanceof DOMMatrixReadOnly, true);
});

test('identity', () => {
	let d = new DOMMatrix();
	assert.equal(d.is2D, true);
	assert.equal(d.isIdentity, true);
	assert.equal(d.toString(), 'matrix(1, 0, 0, 1, 0, 0)');

	d = DOMMatrix.fromMatrix({ is2D: false });
	assert.equal(d.is2D, false);
	assert.equal(d.isIdentity, true);
	assert.equal(
		d.toString(),
		'matrix3d(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)'
	);
});

test('translate()', () => {
	const d = new DOMMatrix();
	d.translateSelf(4, 5);
	assert.equal(d.is2D, true);
	assert.equal(d.toString(), 'matrix(1, 0, 0, 1, 4, 5)');
	d.translateSelf(1, 2, 3);
	assert.equal(d.is2D, false);
	assert.equal(
		d.toString(),
		'matrix3d(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 5, 7, 3, 1)'
	);
});

// From: https://developer.mozilla.org/docs/Web/API/DOMMatrix/DOMMatrix#examples
test('transformPoint() / DOMPoint#matrixTransform()', () => {
	const point = new DOMPoint(5, 4);
	const scaleX = 2;
	const scaleY = 3;
	const translateX = 12;
	const translateY = 8;
	const angle = Math.PI / 2;
	const matrix = new DOMMatrix([
		Math.cos(angle) * scaleX,
		Math.sin(angle) * scaleX,
		-Math.sin(angle) * scaleY,
		Math.cos(angle) * scaleY,
		translateX,
		translateY,
	]);

	const p1 = matrix.transformPoint(point);
	assert.ok(p1 instanceof DOMPoint);
	assert.equal(p1.x, 0);
	assert.equal(p1.y, 18);
	assert.equal(p1.z, 0);
	assert.equal(p1.w, 1);

	const p2 = point.matrixTransform(matrix);
	assert.ok(p1 instanceof DOMPoint);
	assert.equal(p2.x, 0);
	assert.equal(p2.y, 18);
	assert.equal(p2.z, 0);
	assert.equal(p2.w, 1);
});

test('invertSelf()', () => {
	const scaleX = 2;
	const scaleY = 3;
	const translateX = 12;
	const translateY = 8;
	const angle = Math.PI / 2;
	const matrix = new DOMMatrix([
		Math.cos(angle) * scaleX,
		Math.sin(angle) * scaleX,
		-Math.sin(angle) * scaleY,
		Math.cos(angle) * scaleY,
		translateX,
		translateY,
	]);

	matrix.invertSelf();
	assert.equal(
		matrix.toString(),
		'matrix(3.061616997868383e-17, -0.3333333333333333, 0.5, 2.041077998578922e-17, -4.000000000000001, 4)'
	);
});

test.run();
