import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('window');

test('instanceof', () => {
	assert.instance(globalThis, Window);
	assert.equal(Object.prototype.toString.call(globalThis), '[object Window]');

	if (typeof window !== 'undefined') {
		assert.equal(window instanceof Window, true);
		assert.equal(globalThis === window, true);
	}
});

test('throws illegal constructor', () => {
	let err: Error | undefined;
	try {
		new Window();
	} catch (e: any) {
		err = e;
	}
	assert.ok(err);
	assert.equal(err.message, 'Illegal constructor');
});

test('supports global events', () => {
	let gotEvent = false;
	addEventListener('test', (e) => {
		gotEvent = true;
		e.preventDefault();
	});
	const e = new Event('test', { cancelable: true });
	dispatchEvent(e);
	assert.ok(gotEvent);
	assert.ok(e.defaultPrevented);
});

test('atob', () => {
	assert.equal(globalThis.hasOwnProperty('atob'), true);
	assert.equal(atob.length, 1);
	const result = atob('SGVsbG8sIHdvcmxk');
	assert.equal(result, 'Hello, world');
});

test('btoa', () => {
	assert.equal(globalThis.hasOwnProperty('btoa'), true);
	assert.equal(btoa.length, 1);
	const result = btoa('Hello, world');
	assert.equal(result, 'SGVsbG8sIHdvcmxk');
});

test.run();
