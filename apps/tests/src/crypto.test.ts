import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('crypto');

const isNXJS = typeof Switch !== 'undefined';

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

function concat(...buffers: ArrayBuffer[]): ArrayBuffer {
	const size = buffers.reduce((acc, buf) => acc + buf.byteLength, 0);
	let offset = 0;
	const result = new Uint8Array(size);
	for (const buf of buffers) {
		result.set(new Uint8Array(buf), offset);
		offset += buf.byteLength;
	}
	return result.buffer;
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

test("`crypto.subtle.importKey()` with 'raw' format and 'AES-CBC' algorithm, 128-bit key", async () => {
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
	assert.equal(key.extractable, false);
	assert.equal(key.algorithm.name, 'AES-CBC');
	// @ts-expect-error `length` is not defined on `KeyAlgorithm`
	assert.equal(key.algorithm.length, 128);

	// Sorted because bun / deno disagree on the order
	const usages = key.usages.slice().sort();
	assert.equal(usages.length, 2);
	assert.equal(usages[0], 'decrypt');
	assert.equal(usages[1], 'encrypt');
});

test("`crypto.subtle.importKey()` with 'raw' format and 'AES-CBC' algorithm, 256-bit key", async () => {
	const keyData = new Uint8Array([
		194, 180, 97, 245, 222, 0, 208, 29, 177, 74, 94, 95, 124, 172, 123, 89, 168,
		89, 145, 158, 128, 61, 7, 182, 192, 90, 250, 33, 24, 44, 24, 108,
	]);

	const key = await crypto.subtle.importKey('raw', keyData, 'AES-CBC', false, [
		'decrypt',
	]);

	assert.instance(key, CryptoKey);
	assert.equal(key.usages.length, 1);
	assert.equal(key.usages[0], 'decrypt');
	assert.equal(key.algorithm.name, 'AES-CBC');
	// @ts-expect-error `length` is not defined on `KeyAlgorithm`
	assert.equal(key.algorithm.length, 256);
});

test("`crypto.subtle.encrypt()` with 'AES-CBC' algorithm, 128-bit key", async () => {
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

test("`crypto.subtle.encrypt()` with 'AES-CBC' algorithm, 256-bit key", async () => {
	const keyData = new Uint8Array([
		194, 180, 97, 245, 222, 0, 208, 29, 177, 74, 94, 95, 124, 172, 123, 89, 168,
		89, 145, 158, 128, 61, 7, 182, 192, 90, 250, 33, 24, 44, 24, 108,
	]);
	const iv = new Uint8Array([
		199, 76, 237, 213, 127, 137, 216, 106, 243, 237, 191, 146, 158, 226, 56,
		143,
	]);

	const key = await crypto.subtle.importKey(
		'raw',
		keyData,
		{ name: 'AES-CBC' },
		false,
		['encrypt'],
	);

	const ciphertext = await crypto.subtle.encrypt(
		{ name: 'AES-CBC', iv },
		key,
		new TextEncoder().encode('hello').buffer,
	);

	assert.instance(ciphertext, ArrayBuffer);
	assert.equal(toHex(ciphertext), 'da7299d52ef669a5e513b801fb65ef06');
});

test("`crypto.subtle.decrypt()` with 'AES-CBC' algorithm, 128-bit key", async () => {
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

test("`crypto.subtle.decrypt()` with 'AES-CBC' algorithm, 256-bit key", async () => {
	const keyData = new Uint8Array([
		194, 180, 97, 245, 222, 0, 208, 29, 177, 74, 94, 95, 124, 172, 123, 89, 168,
		89, 145, 158, 128, 61, 7, 182, 192, 90, 250, 33, 24, 44, 24, 108,
	]);
	const iv = new Uint8Array([
		199, 76, 237, 213, 127, 137, 216, 106, 243, 237, 191, 146, 158, 226, 56,
		143,
	]);

	const key = await crypto.subtle.importKey(
		'raw',
		keyData,
		{ name: 'AES-CBC' },
		false,
		['decrypt'],
	);

	const plaintext = await crypto.subtle.decrypt(
		{ name: 'AES-CBC', iv },
		key,
		fromHex('da7299d52ef669a5e513b801fb65ef06'),
	);

	assert.instance(plaintext, ArrayBuffer);
	assert.equal(new TextDecoder().decode(new Uint8Array(plaintext)), 'hello');
});

// Non-standard APIs that are only going to work on nx.js
if (isNXJS) {
	test("`crypto.subtle.importKey()` with 'raw' format and 'AES-XTS' algorithm, two 128-bit keys", async () => {
		const key0 = fromHex('0a316b344a7bc7b4db239c61017b86f7');
		const key1 = fromHex('cc4f4df7cb2becb9a307bb17e8eb01f3');

		const key = await crypto.subtle.importKey(
			'raw',
			concat(key0, key1),
			{ name: 'AES-XTS' },
			true,
			['encrypt'],
		);

		assert.instance(key, CryptoKey);
		assert.equal(key.extractable, true);
		assert.equal(key.usages.length, 1);
		assert.equal(key.usages[0], 'encrypt');
		assert.equal(key.algorithm.name, 'AES-XTS');
		// @ts-expect-error `length` is not defined on `KeyAlgorithm`
		assert.equal(key.algorithm.length, 256);
	});

	test("`crypto.subtle.encrypt()` with 'AES-XTS' algorithm, two 128-bit keys (aligned with AES block size)", async () => {
		const key0 = fromHex('0a316b344a7bc7b4db239c61017b86f7');
		const key1 = fromHex('cc4f4df7cb2becb9a307bb17e8eb01f3');

		const key = await crypto.subtle.importKey(
			'raw',
			new BigUint64Array(concat(key0, key1)),
			'AES-XTS',
			true,
			['encrypt'],
		);

		const text = 'This is some text that aligns with 48 bytes!!!!!';
		const plaintext = new TextEncoder().encode(text);
		assert.equal(plaintext.byteLength, 48);

		const ciphertext = await crypto.subtle.encrypt(
			{ name: 'AES-XTS', sectorSize: 16 },
			key,
			plaintext,
		);

		assert.instance(ciphertext, ArrayBuffer);
		assert.equal(
			toHex(ciphertext),
			'b55622812044704bf3f0565ec78d3adfa5fee13aaf030467c2d5c084184989267583c148a883a0ea73d9da63fcf3f4bf',
		);
	});

	test("`crypto.subtle.decrypt()` with 'AES-XTS' algorithm, two 128-bit keys (aligned with AES block size)", async () => {
		const key0 = fromHex('0a316b344a7bc7b4db239c61017b86f7');
		const key1 = fromHex('cc4f4df7cb2becb9a307bb17e8eb01f3');

		const key = await crypto.subtle.importKey(
			'raw',
			concat(key0, key1),
			'AES-XTS',
			true,
			['decrypt'],
		);

		const plaintext = await crypto.subtle.decrypt(
			{ name: 'AES-XTS', sectorSize: 16 },
			key,
			fromHex(
				'b55622812044704bf3f0565ec78d3adfa5fee13aaf030467c2d5c084184989267583c148a883a0ea73d9da63fcf3f4bf',
			),
		);

		assert.instance(plaintext, ArrayBuffer);
		const text = 'This is some text that aligns with 48 bytes!!!!!';
		assert.equal(new TextDecoder().decode(new Uint8Array(plaintext)), text);
	});
}

test.run();
