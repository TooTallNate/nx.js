import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('Error');

test('`Error#stack` include source maps', () => {
	const err = new Error('bad');
	assert.equal(err.stack?.includes('(app:../src/error.ts:'), true);
});

test.run();
