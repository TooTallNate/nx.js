import { test } from '../src/tap';

test('globalThis.addEventListener is a function', (t) => {
	t.equal(
		typeof globalThis.addEventListener,
		'function',
		'addEventListener exists on globalThis',
	);
});

test('globalThis.addEventListener.name is "addEventListener"', (t) => {
	t.equal(
		globalThis.addEventListener.name,
		'addEventListener',
		'function name is addEventListener',
	);
});

test('globalThis.removeEventListener is a function', (t) => {
	t.equal(
		typeof globalThis.removeEventListener,
		'function',
		'removeEventListener exists on globalThis',
	);
});

test('globalThis.dispatchEvent is a function', (t) => {
	t.equal(
		typeof globalThis.dispatchEvent,
		'function',
		'dispatchEvent exists on globalThis',
	);
});

test('globalThis.addEventListener receives dispatched events', (t) => {
	let received = false;
	const handler = () => {
		received = true;
	};
	globalThis.addEventListener('__test-global-dispatch', handler, {
		once: true,
	});
	globalThis.dispatchEvent(new Event('__test-global-dispatch'));
	t.ok(received, 'event received via globalThis.addEventListener');
});

test('globalThis.removeEventListener removes the listener', (t) => {
	let count = 0;
	const handler = () => {
		count++;
	};
	globalThis.addEventListener('__test-global-remove', handler);
	globalThis.dispatchEvent(new Event('__test-global-remove'));
	globalThis.removeEventListener('__test-global-remove', handler);
	globalThis.dispatchEvent(new Event('__test-global-remove'));
	t.equal(count, 1, 'listener not called after removal');
});

test('globalThis.addEventListener once option', (t) => {
	let count = 0;
	globalThis.addEventListener(
		'__test-global-once',
		() => {
			count++;
		},
		{ once: true },
	);
	globalThis.dispatchEvent(new Event('__test-global-once'));
	globalThis.dispatchEvent(new Event('__test-global-once'));
	t.equal(count, 1, 'once listener fires only once');
});

test('globalThis.addEventListener signal option removes on abort', (t) => {
	const controller = new AbortController();
	let count = 0;
	globalThis.addEventListener(
		'__test-global-signal',
		() => {
			count++;
		},
		{ signal: controller.signal },
	);
	globalThis.dispatchEvent(new Event('__test-global-signal'));
	t.equal(count, 1, 'listener called before abort');
	controller.abort();
	globalThis.dispatchEvent(new Event('__test-global-signal'));
	t.equal(count, 1, 'listener not called after abort');
});
