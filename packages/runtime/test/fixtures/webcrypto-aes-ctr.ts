import { test } from '../src/tap';

// Uint8Array hex methods (TC39 stage 4, supported in all target runtimes)
declare global {
	interface Uint8Array {
		toHex(): string;
	}
	interface Uint8ArrayConstructor {
		fromHex(hex: string): Uint8Array;
	}
}

test('AES-CTR-128 encrypt/decrypt with known key', async (t) => {
	const keyData = new Uint8Array(16);
	const counter = new Uint8Array(16);
	counter[15] = 1;
	const plaintext = new TextEncoder().encode('Hello AES-CTR!');

	const key = await crypto.subtle.importKey(
		'raw',
		keyData,
		{ name: 'AES-CTR' },
		true,
		['encrypt', 'decrypt'],
	);

	const encrypted = await crypto.subtle.encrypt(
		{ name: 'AES-CTR', counter, length: 128 },
		key,
		plaintext,
	);

	t.equal(
		encrypted.byteLength,
		plaintext.byteLength,
		'CTR ciphertext same length as plaintext',
	);

	const decrypted = await crypto.subtle.decrypt(
		{ name: 'AES-CTR', counter, length: 128 },
		key,
		encrypted,
	);
	t.equal(
		new TextDecoder().decode(decrypted),
		'Hello AES-CTR!',
		'round trip matches',
	);
});

test('AES-CTR-256 encrypt/decrypt', async (t) => {
	const keyData = new Uint8Array(32);
	const counter = new Uint8Array(16);
	counter[15] = 1;
	const plaintext = new TextEncoder().encode('Hello AES-CTR!');

	const key = await crypto.subtle.importKey(
		'raw',
		keyData,
		{ name: 'AES-CTR' },
		true,
		['encrypt', 'decrypt'],
	);

	const encrypted = await crypto.subtle.encrypt(
		{ name: 'AES-CTR', counter, length: 128 },
		key,
		plaintext,
	);

	const decrypted = await crypto.subtle.decrypt(
		{ name: 'AES-CTR', counter, length: 128 },
		key,
		encrypted,
	);
	t.equal(
		new TextDecoder().decode(decrypted),
		'Hello AES-CTR!',
		'256-bit round trip matches',
	);
});

test('AES-CTR importKey rejects invalid key length', async (t) => {
	let threw = false;
	try {
		await crypto.subtle.importKey('raw', new Uint8Array(7), 'AES-CTR', true, [
			'encrypt',
		]);
	} catch {
		threw = true;
	}
	t.ok(threw, 'invalid key length throws');
});

test('AES-CTR key properties', async (t) => {
	const key = await crypto.subtle.importKey(
		'raw',
		new Uint8Array(16),
		{ name: 'AES-CTR' },
		true,
		['encrypt', 'decrypt'],
	);

	t.equal(key.type, 'secret', 'key type');
	t.equal(key.algorithm.name, 'AES-CTR', 'algorithm name');
	t.equal(key.extractable, true, 'extractable');
	t.deepEqual(
		Array.from(key.usages).sort(),
		['decrypt', 'encrypt'],
		'usages',
	);
});
