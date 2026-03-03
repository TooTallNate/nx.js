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

test('AES-CBC-128 encrypt/decrypt with known key', async (t) => {
	const keyData = new Uint8Array(16); // all zeros
	const iv = new Uint8Array(16); // all zeros
	const plaintext = new TextEncoder().encode('Hello AES-CBC!  '); // 16 bytes

	const key = await crypto.subtle.importKey(
		'raw',
		keyData,
		{ name: 'AES-CBC' },
		true,
		['encrypt', 'decrypt'],
	);

	const encrypted = await crypto.subtle.encrypt(
		{ name: 'AES-CBC', iv },
		key,
		plaintext,
	);

	// Deterministic: zero key + zero IV + known plaintext
	t.equal(encrypted.byteLength, 32, 'ciphertext is 32 bytes (16 + padding)');

	const decrypted = await crypto.subtle.decrypt(
		{ name: 'AES-CBC', iv },
		key,
		encrypted,
	);
	t.equal(
		new TextDecoder().decode(decrypted),
		'Hello AES-CBC!  ',
		'decrypted text matches original',
	);
});

test('AES-CBC-256 encrypt/decrypt with known key', async (t) => {
	const keyData = new Uint8Array(32); // all zeros
	const iv = new Uint8Array(16);
	const plaintext = new TextEncoder().encode('Hello AES-CBC!  ');

	const key = await crypto.subtle.importKey(
		'raw',
		keyData,
		{ name: 'AES-CBC' },
		true,
		['encrypt', 'decrypt'],
	);

	const encrypted = await crypto.subtle.encrypt(
		{ name: 'AES-CBC', iv },
		key,
		plaintext,
	);

	t.equal(encrypted.byteLength, 32, 'ciphertext is 32 bytes');

	const decrypted = await crypto.subtle.decrypt(
		{ name: 'AES-CBC', iv },
		key,
		encrypted,
	);
	t.equal(
		new TextDecoder().decode(decrypted),
		'Hello AES-CBC!  ',
		'round trip matches',
	);
});

test('AES-CBC importKey rejects invalid key length', async (t) => {
	let threw = false;
	try {
		await crypto.subtle.importKey('raw', new Uint8Array(7), 'AES-CBC', true, [
			'encrypt',
		]);
	} catch {
		threw = true;
	}
	t.ok(threw, 'invalid key length throws');
});

test('AES-CBC key properties', async (t) => {
	const key = await crypto.subtle.importKey(
		'raw',
		new Uint8Array(16),
		{ name: 'AES-CBC' },
		true,
		['encrypt', 'decrypt'],
	);

	t.equal(key.type, 'secret', 'key type is secret');
	t.equal(key.algorithm.name, 'AES-CBC', 'algorithm name');
	t.equal(key.extractable, true, 'extractable');
	t.deepEqual(
		Array.from(key.usages).sort(),
		['decrypt', 'encrypt'],
		'usages',
	);
});
