import { test } from '../src/tap';

function hexToBytes(hex: string): Uint8Array {
	const bytes = new Uint8Array(hex.length / 2);
	for (let i = 0; i < hex.length; i += 2) {
		bytes[i / 2] = parseInt(hex.substring(i, i + 2), 16);
	}
	return bytes;
}

test('HKDF RFC 5869 Test Case 1', async (t) => {
	const ikm = hexToBytes('0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b');
	const salt = hexToBytes('000102030405060708090a0b0c');
	const info = hexToBytes('f0f1f2f3f4f5f6f7f8f9');

	const key = await crypto.subtle.importKey(
		'raw',
		ikm,
		{ name: 'HKDF' },
		false,
		['deriveBits', 'deriveKey'],
	);

	const derived = await crypto.subtle.deriveBits(
		{ name: 'HKDF', hash: 'SHA-256', salt, info },
		key,
		336, // 42 bytes
	);

	t.equal(derived.byteLength, 42, 'derived 42 bytes');
	t.equal(
		new Uint8Array(derived).toHex(),
		'3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865',
		'matches RFC 5869 TC1 expected output',
	);
});

test('HKDF RFC 5869 Test Case 3 (empty salt/info)', async (t) => {
	const ikm = hexToBytes('0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b');

	const key = await crypto.subtle.importKey(
		'raw',
		ikm,
		{ name: 'HKDF' },
		false,
		['deriveBits'],
	);

	const derived = await crypto.subtle.deriveBits(
		{
			name: 'HKDF',
			hash: 'SHA-256',
			salt: new Uint8Array(0),
			info: new Uint8Array(0),
		},
		key,
		336,
	);

	t.equal(
		new Uint8Array(derived).toHex(),
		'8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d9d201395faa4b61a96c8',
		'matches RFC 5869 TC3 expected output',
	);
});

test('HKDF key properties', async (t) => {
	const key = await crypto.subtle.importKey(
		'raw',
		hexToBytes('0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b'),
		{ name: 'HKDF' },
		false,
		['deriveBits', 'deriveKey'],
	);

	t.equal(key.type, 'secret', 'key type');
	t.equal(key.extractable, false, 'not extractable');
	t.equal(key.algorithm.name, 'HKDF', 'algorithm name');
	t.deepEqual(
		Array.from(key.usages).sort(),
		['deriveBits', 'deriveKey'],
		'usages',
	);
});

test('HKDF deriveKey to AES-CBC', async (t) => {
	const ikm = hexToBytes('0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b');
	const salt = hexToBytes('000102030405060708090a0b0c');
	const info = hexToBytes('f0f1f2f3f4f5f6f7f8f9');

	const key = await crypto.subtle.importKey(
		'raw',
		ikm,
		{ name: 'HKDF' },
		false,
		['deriveBits', 'deriveKey'],
	);

	const aesKey = await crypto.subtle.deriveKey(
		{ name: 'HKDF', hash: 'SHA-256', salt, info },
		key,
		{ name: 'AES-CBC', length: 256 },
		true,
		['encrypt', 'decrypt'],
	);

	t.equal(aesKey.type, 'secret', 'derived key type');
	t.equal(aesKey.algorithm.name, 'AES-CBC', 'derived key algorithm');
	t.equal(
		(aesKey.algorithm as AesKeyAlgorithm).length,
		256,
		'derived key length',
	);

	// Round trip with derived key
	const iv = new Uint8Array(16);
	const plaintext = new TextEncoder().encode('Hello, HKDF!');
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
		'Hello, HKDF!',
		'derived key encrypt/decrypt round trip',
	);
});
