import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('crypto');

function toHex(a: ArrayBuffer) {
	const u = new Uint8Array(a);
	const s = [];
	for (let i = 0; i < u.length; i++) {
		s.push(u[i].toString(16).padStart(2, '0'));
	}
	return s.join('');
}

test('`crypto.getRandomValues()`', () => {
	const arr = new Uint8Array(5);
	assert.equal(toHex(arr.buffer), '0000000000');
	crypto.getRandomValues(arr);
	assert.not.equal(toHex(arr.buffer), '0000000000');
});

test("`crypto.subtle.digest('sha-1')`", async () => {
	const data = new TextEncoder().encode('hello');
	const digest = await crypto.subtle.digest('sha-1', data);
	assert.equal(toHex(digest), 'aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d');
});

test("`crypto.subtle.digest('sha-256')`", async () => {
	const data = new TextEncoder().encode('hello');
	const digest = await crypto.subtle.digest('sha-256', data);
	assert.equal(
		toHex(digest),
		'2cf24dba5fb0a30e26e83b2ac5b9e29e1b161e5c1fa7425e73043362938b9824',
	);
});

test.run();
