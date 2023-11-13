import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('window');

test('instanceof', () => {
	assert.equal(window instanceof Window, true);
	assert.equal(globalThis instanceof Window, true);
	assert.equal(globalThis === window, true);
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

test.run();
