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

test('PBKDF2 RFC 6070 test vector (SHA-1)', async (t) => {
	const password = new TextEncoder().encode('password');
	const salt = new TextEncoder().encode('salt');

	const key = await crypto.subtle.importKey(
		'raw',
		password,
		{ name: 'PBKDF2' },
		false,
		['deriveBits', 'deriveKey'],
	);

	const derived = await crypto.subtle.deriveBits(
		{ name: 'PBKDF2', salt, iterations: 4096, hash: 'SHA-1' },
		key,
		160,
	);

	t.equal(
		new Uint8Array(derived).toHex(),
		'4b007901b765489abead49d926f721d065a429c1',
		'matches RFC 6070 expected output',
	);
});

test('PBKDF2 with SHA-256', async (t) => {
	const password = new TextEncoder().encode('password');
	const salt = new TextEncoder().encode('salt');

	const key = await crypto.subtle.importKey(
		'raw',
		password,
		{ name: 'PBKDF2' },
		false,
		['deriveBits'],
	);

	const derived = await crypto.subtle.deriveBits(
		{ name: 'PBKDF2', salt, iterations: 1, hash: 'SHA-256' },
		key,
		256,
	);

	t.equal(derived.byteLength, 32, 'derived 32 bytes');
	t.equal(
		new Uint8Array(derived).toHex(),
		'120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b',
		'PBKDF2-SHA256 known output',
	);
});

test('PBKDF2 key properties', async (t) => {
	const key = await crypto.subtle.importKey(
		'raw',
		new TextEncoder().encode('password'),
		{ name: 'PBKDF2' },
		false,
		['deriveBits', 'deriveKey'],
	);

	t.equal(key.type, 'secret', 'key type');
	t.equal(key.extractable, false, 'not extractable');
	t.equal(key.algorithm.name, 'PBKDF2', 'algorithm name');
	t.deepEqual(
		Array.from(key.usages).sort(),
		['deriveBits', 'deriveKey'],
		'usages',
	);
});

test('PBKDF2 deriveKey to AES-CBC round trip', async (t) => {
	const key = await crypto.subtle.importKey(
		'raw',
		new TextEncoder().encode('password'),
		{ name: 'PBKDF2' },
		false,
		['deriveBits', 'deriveKey'],
	);

	const aesKey = await crypto.subtle.deriveKey(
		{
			name: 'PBKDF2',
			salt: new TextEncoder().encode('mysalt'),
			iterations: 1000,
			hash: 'SHA-256',
		},
		key,
		{ name: 'AES-CBC', length: 256 },
		true,
		['encrypt', 'decrypt'],
	);

	t.equal(aesKey.type, 'secret', 'derived key type');
	t.equal(aesKey.algorithm.name, 'AES-CBC', 'derived key algorithm');
	t.equal((aesKey.algorithm as AesKeyAlgorithm).length, 256, 'derived key length');

	const iv = new Uint8Array(16);
	const plaintext = new TextEncoder().encode('Hello, PBKDF2!');
	const ciphertext = await crypto.subtle.encrypt(
		{ name: 'AES-CBC', iv },
		aesKey,
		plaintext,
	);
	const decrypted = await crypto.subtle.decrypt(
		{ name: 'AES-CBC', iv },
		aesKey,
		ciphertext,
	);
	t.equal(
		new TextDecoder().decode(decrypted),
		'Hello, PBKDF2!',
		'derived key encrypt/decrypt round trip',
	);
});
