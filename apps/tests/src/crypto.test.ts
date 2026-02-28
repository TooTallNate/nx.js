import { suite } from 'uvu';
import * as assert from 'uvu/assert';
import { toHex, fromHex, concat } from './util.ts';

const test = suite('crypto');

// #region crypto.getRandomValues()

test('`crypto.getRandomValues()`', () => {
	const arr = new Uint8Array(5);
	assert.equal(toHex(arr.buffer), '0000000000');
	crypto.getRandomValues(arr);
	assert.not.equal(toHex(arr.buffer), '0000000000');
});

test('`crypto.getRandomValues()` with 0 arguments', () => {
	let err: Error | undefined;
	try {
		// @ts-expect-error Intentionally passing 0 arguments
		crypto.getRandomValues();
	} catch (e: any) {
		err = e;
	}
	assert.ok(err);
	assert.equal(
		err.message,
		"Failed to execute 'getRandomValues' on 'Crypto': 1 argument required, but only 0 present",
	);
});

// #endregion

// #region crypto.subtle.digest()

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

// #endregion

// #region crypto.subtle.importKey()

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

	const key = await crypto.subtle.importKey('raw', keyData, 'AES-CBC', true, [
		'decrypt',
	]);

	assert.instance(key, CryptoKey);
	assert.equal(key.extractable, true);
	assert.equal(key.usages.length, 1);
	assert.equal(key.usages[0], 'decrypt');
	assert.equal(key.algorithm.name, 'AES-CBC');
	assert.equal(key.algorithm.length, 256);
});

test("`crypto.subtle.importKey()` with 'raw' format and 'AES-CBC' algorithm, invalid key length", async () => {
	let err: Error | undefined;
	try {
		await crypto.subtle.importKey('raw', new Uint8Array(), 'AES-CBC', true, []);
	} catch (_err: any) {
		err = _err;
	}

	assert.ok(err);
	assert.instance(err, Error);
	assert.equal(err.message, 'Invalid key length');
});

test("`crypto.subtle.importKey()` with 'raw' format and 'AES-CTR' algorithm, 128-bit key", async () => {
	const keyData = new Uint8Array([
		188, 136, 184, 200, 227, 200, 149, 203, 33, 186, 60, 145, 54, 19, 92, 88,
	]);

	const key = await crypto.subtle.importKey(
		'raw',
		keyData,
		{ name: 'AES-CTR' },
		false,
		['encrypt', 'decrypt'],
	);

	assert.instance(key, CryptoKey);
	assert.equal(key.extractable, false);
	assert.equal(key.algorithm.name, 'AES-CTR');
	assert.equal(key.algorithm.length, 128);

	// Sorted because bun / deno disagree on the order
	const usages = key.usages.slice().sort();
	assert.equal(usages.length, 2);
	assert.equal(usages[0], 'decrypt');
	assert.equal(usages[1], 'encrypt');
});

test("`crypto.subtle.importKey()` with 'raw' format and 'AES-CTR' algorithm, invalid key length", async () => {
	let err: Error | undefined;
	try {
		await crypto.subtle.importKey('raw', new Uint8Array(), 'AES-CTR', true, []);
	} catch (_err: any) {
		err = _err;
	}

	assert.ok(err);
	assert.instance(err, Error);
	assert.equal(err.message, 'Invalid key length');
});

// #endregion

// #region crypto.subtle.encrypt()

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

test("`crypto.subtle.encrypt()` with 'AES-CTR' algorithm, 128-bit key", async () => {
	const keyData = new Uint8Array([
		188, 136, 184, 200, 227, 200, 149, 203, 33, 186, 60, 145, 54, 19, 92, 88,
	]);
	const counter = new Uint8Array([
		38, 89, 172, 231, 98, 165, 172, 212, 137, 184, 41, 162, 105, 26, 119, 158,
	]);

	const key = await crypto.subtle.importKey('raw', keyData, 'AES-CTR', false, [
		'encrypt',
	]);

	const ciphertext = await crypto.subtle.encrypt(
		{ name: 'AES-CTR', counter, length: 64 },
		key,
		new TextEncoder().encode('hello'),
	);

	assert.instance(ciphertext, ArrayBuffer);
	assert.equal(toHex(ciphertext), '3476c80fdb');
});

test("`crypto.subtle.encrypt()` with 'AES-CTR' algorithm, 256-bit key", async () => {
	const keyData = new Uint8Array([
		194, 180, 97, 245, 222, 0, 208, 29, 177, 74, 94, 95, 124, 172, 123, 89, 168,
		89, 145, 158, 128, 61, 7, 182, 192, 90, 250, 33, 24, 44, 24, 108,
	]);
	const counter = new Uint8Array([
		38, 89, 172, 231, 98, 165, 172, 212, 137, 184, 41, 162, 105, 26, 119, 158,
	]);

	const key = await crypto.subtle.importKey('raw', keyData, 'AES-CTR', false, [
		'encrypt',
	]);

	const ciphertext = await crypto.subtle.encrypt(
		{ name: 'AES-CTR', counter, length: 32 },
		key,
		new TextEncoder().encode(
			'this is some longer text that is larger than the block size',
		),
	);

	assert.instance(ciphertext, ArrayBuffer);
	assert.equal(
		toHex(ciphertext),
		'c3055adf88fb00a51a47fc84efa003c1ed668082a61326bed6744ab1fd1eed2e5ffd8854fbb383da093a7600d83e70bdbc05ac13348caf9efaf3cf',
	);
});

// #endregion

// #region crypto.subtle.decrypt()

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

test("`crypto.subtle.decrypt()` with 'AES-CTR' algorithm, 128-bit key", async () => {
	const keyData = new Uint8Array([
		188, 136, 184, 200, 227, 200, 149, 203, 33, 186, 60, 145, 54, 19, 92, 88,
	]);
	const counter = new Uint8Array([
		38, 89, 172, 231, 98, 165, 172, 212, 137, 184, 41, 162, 105, 26, 119, 158,
	]);

	const key = await crypto.subtle.importKey('raw', keyData, 'AES-CTR', false, [
		'decrypt',
	]);

	const plaintext = await crypto.subtle.decrypt(
		{ name: 'AES-CTR', counter, length: 64 },
		key,
		fromHex('3476c80fdb'),
	);

	assert.instance(plaintext, ArrayBuffer);
	assert.equal(new TextDecoder().decode(new Uint8Array(plaintext)), 'hello');
});

// #endregion

// #region Concurrent operations (thread-safety)

test('concurrent `crypto.subtle.encrypt()` with same AES-CBC 128-bit key', async () => {
	const keyData = new Uint8Array([
		188, 136, 184, 200, 227, 200, 149, 203, 33, 186, 60, 145, 54, 19, 92, 88,
	]);

	const key = await crypto.subtle.importKey('raw', keyData, 'AES-CBC', false, [
		'encrypt',
	]);

	// Different IVs for each concurrent operation
	const ivs = [
		new Uint8Array([
			38, 89, 172, 231, 98, 165, 172, 212, 137, 184, 41, 162, 105, 26, 119, 158,
		]),
		new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]),
		new Uint8Array([16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1]),
		new Uint8Array([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]),
	];

	const plaintext = new TextEncoder().encode('hello');

	// Fire all encryptions concurrently with the same key but different IVs
	const results = await Promise.all(
		ivs.map((iv) =>
			crypto.subtle.encrypt({ name: 'AES-CBC', iv }, key, plaintext),
		),
	);

	// Each result should be a valid ArrayBuffer
	for (const result of results) {
		assert.instance(result, ArrayBuffer);
		assert.equal(result.byteLength, 16); // one AES block with PKCS#7 padding
	}

	// All results should be different (different IVs = different ciphertexts)
	const hexResults = results.map((r) => toHex(r));
	const unique = new Set(hexResults);
	assert.equal(
		unique.size,
		ivs.length,
		'Each IV should produce a unique ciphertext',
	);

	// First IV matches the known value from the non-concurrent test
	assert.equal(hexResults[0], '4b4fddd4b88f2e6a36500f89aa177d0d');
});

test('concurrent `crypto.subtle.decrypt()` with same AES-CBC 128-bit key', async () => {
	const keyData = new Uint8Array([
		188, 136, 184, 200, 227, 200, 149, 203, 33, 186, 60, 145, 54, 19, 92, 88,
	]);

	const key = await crypto.subtle.importKey('raw', keyData, 'AES-CBC', false, [
		'encrypt',
		'decrypt',
	]);

	const iv = new Uint8Array([
		38, 89, 172, 231, 98, 165, 172, 212, 137, 184, 41, 162, 105, 26, 119, 158,
	]);

	// Encrypt the same plaintext multiple times with different data
	const plaintexts = ['hello', 'world', 'foo!!', 'bar!!'];
	const ciphertexts = await Promise.all(
		plaintexts.map((p) =>
			crypto.subtle.encrypt(
				{ name: 'AES-CBC', iv },
				key,
				new TextEncoder().encode(p),
			),
		),
	);

	// Now decrypt all concurrently
	const decrypted = await Promise.all(
		ciphertexts.map((ct) =>
			crypto.subtle.decrypt({ name: 'AES-CBC', iv }, key, ct),
		),
	);

	for (let i = 0; i < plaintexts.length; i++) {
		assert.equal(
			new TextDecoder().decode(new Uint8Array(decrypted[i])),
			plaintexts[i],
		);
	}
});

test('concurrent `crypto.subtle.encrypt()` with same AES-CTR 128-bit key', async () => {
	const keyData = new Uint8Array([
		188, 136, 184, 200, 227, 200, 149, 203, 33, 186, 60, 145, 54, 19, 92, 88,
	]);

	const key = await crypto.subtle.importKey('raw', keyData, 'AES-CTR', false, [
		'encrypt',
	]);

	const counters = [
		new Uint8Array([
			38, 89, 172, 231, 98, 165, 172, 212, 137, 184, 41, 162, 105, 26, 119, 158,
		]),
		new Uint8Array([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]),
		new Uint8Array([16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1]),
		new Uint8Array([0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]),
	];

	const plaintext = new TextEncoder().encode('hello');

	const results = await Promise.all(
		counters.map((counter) =>
			crypto.subtle.encrypt(
				{ name: 'AES-CTR', counter, length: 64 },
				key,
				plaintext,
			),
		),
	);

	const hexResults = results.map((r) => toHex(r));
	const unique = new Set(hexResults);
	assert.equal(
		unique.size,
		counters.length,
		'Each counter should produce a unique ciphertext',
	);

	// First counter matches the known value
	assert.equal(hexResults[0], '3476c80fdb');
});

// #endregion

// #region nx.js specific APIs

// Non-standard APIs that are only going to work on nx.js
if (process.env.NXJS === '1') {
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
		assert.equal(key.algorithm.length, 256);
	});

	test("`crypto.subtle.importKey()` with 'raw' format and 'AES-XTS' algorithm, invalid key length", async () => {
		let err: Error | undefined;
		try {
			await crypto.subtle.importKey(
				'raw',
				new Uint8Array(),
				'AES-XTS',
				true,
				[],
			);
		} catch (_err: any) {
			err = _err;
		}

		assert.ok(err);
		assert.instance(err, Error);
		assert.equal(err.message, 'Invalid key length');
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

// #endregion

test.run();
