import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('Error');

test('`Error#stack` include source maps', () => {
	const err = new Error('bad');
	assert.equal(err.stack?.includes('(app:../src/error.test.ts:'), true);
});

test('Emit "unhandledrejection" event when an unhandled promise rejection occurs immediately', async () => {
	const evPromise = new Promise<PromiseRejectionEvent>((r) =>
		addEventListener(
			'unhandledrejection',
			(ev) => {
				ev.preventDefault();
				r(ev);
			},
			{ once: true },
		),
	);

	async function immediatelyThrow() {
		throw new Error('should be caught');
	}

	const promise = immediatelyThrow();

	const ev = await evPromise;
	assert.equal(ev.promise, promise);
	assert.ok(ev.reason instanceof Error);
	assert.equal(ev.reason.message, 'should be caught');
});

test('Emit "unhandledrejection" event when an unhandled promise rejection occurs asynchronously', async () => {
	const evPromise = new Promise<PromiseRejectionEvent>((r) =>
		addEventListener(
			'unhandledrejection',
			(ev) => {
				ev.preventDefault();
				r(ev);
			},
			{ once: true },
		),
	);

	async function throwAfterTick() {
		await Promise.resolve();
		throw new Error('should be caught');
	}

	const promise = throwAfterTick();

	const ev = await evPromise;
	assert.equal(ev.promise, promise);
	assert.ok(ev.reason instanceof Error);
	assert.equal(ev.reason.message, 'should be caught');
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
