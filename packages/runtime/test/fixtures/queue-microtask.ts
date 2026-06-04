import { test } from '../src/tap';

// Regression: raw V8 does not install a `queueMicrotask` global (it is a
// Blink/Node binding), so after the QuickJS -> V8 migration the global was
// missing even though QuickJS had provided it natively. It is now bound in C
// via Isolate::EnqueueMicrotask.

test('queueMicrotask is a global function', (t) => {
	t.equal(typeof queueMicrotask, 'function', 'queueMicrotask is a function');
});

test('queueMicrotask runs the callback', async (t) => {
	const ran = await new Promise<boolean>((resolve) => {
		queueMicrotask(() => resolve(true));
	});
	t.ok(ran, 'callback was invoked');
});

test('queueMicrotask runs after the current synchronous task', async (t) => {
	const order: string[] = [];
	await new Promise<void>((resolve) => {
		queueMicrotask(() => {
			order.push('microtask');
			resolve();
		});
		order.push('sync');
	});
	t.deepEqual(order, ['sync', 'microtask'], 'sync code runs before microtask');
});

test('queueMicrotask runs before setTimeout (macrotask)', async (t) => {
	const order: string[] = [];
	await new Promise<void>((resolve) => {
		setTimeout(() => {
			order.push('timeout');
			resolve();
		}, 0);
		queueMicrotask(() => {
			order.push('microtask');
		});
	});
	t.deepEqual(
		order,
		['microtask', 'timeout'],
		'microtask drains before the next macrotask',
	);
});

test('queueMicrotask preserves FIFO order', async (t) => {
	const order: number[] = [];
	await new Promise<void>((resolve) => {
		queueMicrotask(() => order.push(1));
		queueMicrotask(() => order.push(2));
		queueMicrotask(() => {
			order.push(3);
			resolve();
		});
	});
	t.deepEqual(order, [1, 2, 3], 'microtasks run in enqueue order');
});

test('queueMicrotask throws on a non-function argument', (t) => {
	t.throws(
		// @ts-expect-error intentionally wrong argument type
		() => queueMicrotask(123),
		/Function/,
		'throws a TypeError-like message when arg is not callable',
	);
});
