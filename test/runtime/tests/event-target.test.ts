import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('EventTarget');

test('A constructed EventTarget can be used as expected', () => {
	const target = new EventTarget();
	const event = new Event('foo', { bubbles: true, cancelable: false });
	let callCount = 0;

	function listener(e: Event) {
		assert.equal(e, event);
		++callCount;
	}

	target.addEventListener('foo', listener);

	target.dispatchEvent(event);
	assert.equal(callCount, 1);

	target.dispatchEvent(event);
	assert.equal(callCount, 2);

	target.removeEventListener('foo', listener);
	target.dispatchEvent(event);
	assert.equal(callCount, 2);
});

test('EventTarget can be subclassed', () => {
	class NicerEventTarget extends EventTarget {
		on(...args: any[]) {
			// @ts-expect-error
			this.addEventListener(...args);
		}

		off(...args: any[]) {
			// @ts-expect-error
			this.removeEventListener(...args);
		}

		dispatch(type: string, detail: string) {
			this.dispatchEvent(new CustomEvent(type, { detail }));
		}
	}

	const target = new NicerEventTarget();
	const detail = 'some data';
	let callCount = 0;

	function listener(e: CustomEvent) {
		assert.equal(e.detail, detail);
		++callCount;
	}

	target.on('foo', listener);

	target.dispatch('foo', detail);
	assert.equal(callCount, 1);

	target.dispatch('foo', detail);
	assert.equal(callCount, 2);

	target.off('foo', listener);
	target.dispatch('foo', detail);
	assert.equal(callCount, 2);
});

test.run();
