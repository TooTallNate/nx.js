import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('Error');

test('`Error#stack` include source maps', () => {
	const err = new Error('bad');
	assert.equal(err.stack?.includes('(app:../src/error.test.ts:'), true);
});

test('Catch immediately thrown errors in async function', async () => {
	async function immediatelyThrow() {
		throw new Error('should be caught');
	}
	let err: Error | undefined;
	try {
		await immediatelyThrow();
	} catch (_err: any) {
		err = _err;
	}
	assert.ok(err);
	assert.equal(err.message, 'should be caught');
});

test.run();
