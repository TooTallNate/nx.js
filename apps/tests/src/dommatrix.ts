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
	const d = new DOMMatrix();
	assert.equal(d.is2D, true);
	assert.equal(d.toString(), 'matrix(1, 0, 0, 1, 0, 0)');
});

test.run();
