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

test('translate', () => {
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

test.run();
