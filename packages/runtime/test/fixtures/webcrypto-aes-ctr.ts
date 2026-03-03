import { test } from '../src/tap';

function toHex(buf: ArrayBuffer): string {
	const bytes = new Uint8Array(buf);
	let hex = '';
	for (let i = 0; i < bytes.length; i++) {
		hex += bytes[i].toString(16).padStart(2, '0');
	}
	return hex;
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
