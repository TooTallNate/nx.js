import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('navigator');

test('instanceof', () => {
	assert.equal(navigator instanceof Navigator, true);
});

test('throws illegal constructor', () => {
	let err: Error | undefined;
	try {
		new Navigator();
	} catch (e: any) {
		err = e;
	}
	assert.ok(err);
	assert.equal(err.message, 'Illegal constructor');
});

test.run();
