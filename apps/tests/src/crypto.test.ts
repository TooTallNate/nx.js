import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('crypto');

function toHex(arr: ArrayBuffer) {
	return Array.from(new Uint8Array(arr))
		.map((v) => v.toString(16).padStart(2, '0'))
		.join('');
}

function fromHex(hex: string): ArrayBuffer {
	const arr = new Uint8Array(hex.length / 2);
	for (let i = 0; i < hex.length; i += 2) {
		arr[i / 2] = parseInt(hex.substr(i, 2), 16);
	}
	return arr.buffer;
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

test("`crypto.subtle.importKey()` with 'raw' format and 'AES-CBC' algorithm`", async () => {
	const keyData = new Uint8Array([
		188, 136, 184, 200, 227, 200, 149, 203, 33, 186, 60, 145, 54, 19, 92, 88,
	]);

	const key = await crypto.subtle.importKey(
		'raw',
		keyData,
		{ name: 'AES-CBC' },
		false,
		['encrypt', 'decrypt'],
	);

	assert.instance(key, CryptoKey);
	assert.equal(key.algorithm.name, 'AES-CBC');
	// @ts-expect-error `length` is not defined on `KeyAlgorithm`
	assert.equal(key.algorithm.length, 128);
});

test("`crypto.subtle.encrypt()` with 'AES-CBC' algorithm`", async () => {
	const keyData = new Uint8Array([
		188, 136, 184, 200, 227, 200, 149, 203, 33, 186, 60, 145, 54, 19, 92, 88,
	]);
	const iv = new Uint8Array([
		38, 89, 172, 231, 98, 165, 172, 212, 137, 184, 41, 162, 105, 26, 119, 158,
	]);

	const key = await crypto.subtle.importKey(
		'raw',
		keyData,
		{ name: 'AES-CBC' },
		false,
		['encrypt'],
	);

	const ciphertext = await crypto.subtle.encrypt(
		{ name: 'AES-CBC', iv: iv.buffer },
		key,
		new TextEncoder().encode('hello'),
	);

	assert.instance(ciphertext, ArrayBuffer);
	assert.equal(toHex(ciphertext), '4b4fddd4b88f2e6a36500f89aa177d0d');
});

test("`crypto.subtle.decrypt()` with 'AES-CBC' algorithm`", async () => {
	const keyData = new Uint8Array([
		188, 136, 184, 200, 227, 200, 149, 203, 33, 186, 60, 145, 54, 19, 92, 88,
	]);
	const iv = new Uint8Array([
		38, 89, 172, 231, 98, 165, 172, 212, 137, 184, 41, 162, 105, 26, 119, 158,
	]);

	const key = await crypto.subtle.importKey('raw', keyData, 'AES-CBC', false, [
		'decrypt',
	]);

	const plaintext = await crypto.subtle.decrypt(
		{ name: 'AES-CBC', iv },
		key,
		fromHex('4b4fddd4b88f2e6a36500f89aa177d0d'),
	);

	assert.instance(plaintext, ArrayBuffer);
	assert.equal(new TextDecoder().decode(new Uint8Array(plaintext)), 'hello');
});

test.run();
