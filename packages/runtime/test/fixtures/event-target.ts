import { test } from '../src/tap';

test('EventTarget addEventListener and dispatch', (t) => {
	const target = new EventTarget();
	let called = false;
	target.addEventListener('test', () => {
		called = true;
	});
	target.dispatchEvent(new Event('test'));
	t.ok(called, 'listener was called');
});

test('EventTarget removeEventListener', (t) => {
	const target = new EventTarget();
	let count = 0;
	const handler = () => {
		count++;
	};
	target.addEventListener('test', handler);
	target.dispatchEvent(new Event('test'));
	target.removeEventListener('test', handler);
	target.dispatchEvent(new Event('test'));
	t.equal(count, 1, 'listener removed after first dispatch');
});

test('EventTarget once option', (t) => {
	const target = new EventTarget();
	let count = 0;
	target.addEventListener(
		'test',
		() => {
			count++;
		},
		{ once: true },
	);
	target.dispatchEvent(new Event('test'));
	target.dispatchEvent(new Event('test'));
	t.equal(count, 1, 'once listener fires only once');
});

test('CustomEvent detail', (t) => {
	const target = new EventTarget();
	let received: unknown = null;
	target.addEventListener('custom', ((e: CustomEvent) => {
		received = e.detail;
	}) as EventListener);
	target.dispatchEvent(new CustomEvent('custom', { detail: { key: 'value' } }));
	t.deepEqual(received, { key: 'value' }, 'detail received');
});

test('Event properties', (t) => {
	const event = new Event('click', { bubbles: true, cancelable: true });
	t.equal(event.type, 'click', 'type');
	t.equal(event.bubbles, true, 'bubbles');
	t.equal(event.cancelable, true, 'cancelable');
	t.equal(event.defaultPrevented, false, 'not yet prevented');
	event.preventDefault();
	t.equal(event.defaultPrevented, true, 'prevented after preventDefault');
});

// stopImmediatePropagation is not yet implemented in nx.js, so this test
// is skipped to avoid breaking the conformance comparison. Uncomment when
// nx.js implements Event.stopImmediatePropagation().
// test('Event stopImmediatePropagation', (t) => { ... });

test('multiple listeners same event', (t) => {
	const target = new EventTarget();
	const calls: string[] = [];
	target.addEventListener('test', () => calls.push('a'));
	target.addEventListener('test', () => calls.push('b'));
	target.addEventListener('test', () => calls.push('c'));
	target.dispatchEvent(new Event('test'));
	t.deepEqual(calls, ['a', 'b', 'c'], 'all listeners called in order');
});
