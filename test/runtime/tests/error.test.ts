import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('Error');

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

test('Catch asynchronously thrown errors in async function', async () => {
	async function throwAfterTick() {
		await Promise.resolve();
		throw new Error('should be caught');
	}
	let err: Error | undefined;
	try {
		await throwAfterTick();
	} catch (_err: any) {
		err = _err;
	}
	assert.ok(err);
	assert.equal(err.message, 'should be caught');
});

test.run();
