import { suite } from 'uvu';
import * as assert from 'uvu/assert';
import { toHex, fromHex, concat } from './util';

const test = suite('WebCrypto AES-XTS');

// AES-XTS is an nx.js-specific (non-standard) WebCrypto algorithm

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

test.run();
