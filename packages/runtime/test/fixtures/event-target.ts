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

test('Event stopImmediatePropagation', (t) => {
	const target = new EventTarget();
	const order: number[] = [];
	target.addEventListener('test', (e) => {
		order.push(1);
		e.stopImmediatePropagation();
	});
	target.addEventListener('test', () => {
		order.push(2);
	});
	target.dispatchEvent(new Event('test'));
	t.deepEqual(
		order,
		[1],
		'second listener not called after stopImmediatePropagation',
	);
});

test('Event stopPropagation does not throw', (t) => {
	const event = new Event('test');
	t.doesNotThrow(() => event.stopPropagation(), 'stopPropagation is no-op');
});

test('Event composedPath returns target', (t) => {
	const target = new EventTarget();
	let path: EventTarget[] = [];
	target.addEventListener('test', (e) => {
		path = e.composedPath();
	});
	target.dispatchEvent(new Event('test'));
	t.equal(path.length, 1, 'composedPath has one element');
	t.equal(path[0], target, 'composedPath contains target');
});

test('Event composedPath empty before dispatch', (t) => {
	const event = new Event('test');
	t.deepEqual(event.composedPath(), [], 'empty before dispatch');
});

test('Event initEvent re-initializes', (t) => {
	const event = new Event('original', { bubbles: true, cancelable: true });
	event.initEvent('changed', false, false);
	t.equal(event.type, 'changed', 'type changed');
	t.equal(event.bubbles, false, 'bubbles changed');
	t.equal(event.cancelable, false, 'cancelable changed');
});

test('Event timeStamp is set', (t) => {
	const event = new Event('test');
	t.equal(typeof event.timeStamp, 'number', 'timeStamp is number');
	t.ok(event.timeStamp >= 0, 'timeStamp is non-negative');
});

test('Event composed option', (t) => {
	const event = new Event('test', { composed: true });
	t.equal(event.composed, true, 'composed is true when set');
	const event2 = new Event('test');
	t.equal(event2.composed, false, 'composed defaults to false');
});

test('addEventListener signal option removes on abort', (t) => {
	const target = new EventTarget();
	const controller = new AbortController();
	let count = 0;
	target.addEventListener(
		'test',
		() => {
			count++;
		},
		{ signal: controller.signal },
	);
	target.dispatchEvent(new Event('test'));
	t.equal(count, 1, 'listener called before abort');
	controller.abort();
	target.dispatchEvent(new Event('test'));
	t.equal(count, 1, 'listener not called after abort');
});

test('addEventListener signal already aborted', (t) => {
	const target = new EventTarget();
	const controller = new AbortController();
	controller.abort();
	let called = false;
	target.addEventListener(
		'test',
		() => {
			called = true;
		},
		{ signal: controller.signal },
	);
	target.dispatchEvent(new Event('test'));
	t.notOk(called, 'listener not added when signal already aborted');
});

test('multiple listeners same event', (t) => {
	const target = new EventTarget();
	const calls: string[] = [];
	target.addEventListener('test', () => calls.push('a'));
	target.addEventListener('test', () => calls.push('b'));
	target.addEventListener('test', () => calls.push('c'));
	target.dispatchEvent(new Event('test'));
	t.deepEqual(calls, ['a', 'b', 'c'], 'all listeners called in order');
});
