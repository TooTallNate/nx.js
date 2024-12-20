import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('crypto');

function toHex(arr: ArrayBuffer) {
	return Array.from(new Uint8Array(arr))
		.map((v) => v.toString(16).padStart(2, '0'))
		.join('');
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

test("`crypto.subtle.digest('sha-384')`", async () => {
	const data = new TextEncoder().encode('hello');
	const digest = await crypto.subtle.digest('sha-384', data);
	assert.equal(
		toHex(digest),
		'59e1748777448c69de6b800d7a33bbfb9ff1b463e44354c3553bcdb9c666fa90125a3c79f90397bdf5f6a13de828684f',
	);
});

test("`crypto.subtle.digest('sha-512')`", async () => {
	const data = new TextEncoder().encode('hello');
	const digest = await crypto.subtle.digest('sha-512', data);
	assert.equal(
		toHex(digest),
		'9b71d224bd62f3785d96d46ad3ea3d73319bfbc2890caadae2dff72519673ca72323c3d99ba5c11d7c7acc6e14b8c5da0c4663475c2e5c3adef46f73bcdec043',
	);
});

test.run();
