import { test } from '../src/tap';

test('RSA-OAEP encrypt/decrypt round trip', async (t) => {
	const pair = await crypto.subtle.generateKey(
		{
			name: 'RSA-OAEP',
			modulusLength: 2048,
			publicExponent: new Uint8Array([1, 0, 1]),
			hash: 'SHA-256',
		},
		true,
		['encrypt', 'decrypt'],
	);

	t.equal(pair.publicKey.type, 'public', 'public key type');
	t.equal(pair.privateKey.type, 'private', 'private key type');
	t.equal(pair.publicKey.algorithm.name, 'RSA-OAEP', 'public algo');
	t.equal(pair.privateKey.algorithm.name, 'RSA-OAEP', 'private algo');
	t.equal(
		(pair.publicKey.algorithm as RsaHashedKeyAlgorithm).hash.name,
		'SHA-256',
		'hash name',
	);
	t.equal(
		(pair.publicKey.algorithm as RsaHashedKeyAlgorithm).modulusLength,
		2048,
		'modulus length',
	);

	const plaintext = new TextEncoder().encode('Hello RSA-OAEP!');
	const encrypted = await crypto.subtle.encrypt(
		{ name: 'RSA-OAEP' },
		pair.publicKey,
		plaintext,
	);

	t.equal(encrypted.byteLength, 256, 'ciphertext is 256 bytes for 2048-bit key');

	const decrypted = await crypto.subtle.decrypt(
		{ name: 'RSA-OAEP' },
		pair.privateKey,
		encrypted,
	);
	t.equal(
		new TextDecoder().decode(decrypted),
		'Hello RSA-OAEP!',
		'round trip matches',
	);
});

test('RSA-OAEP wrong key fails', async (t) => {
	const pair1 = await crypto.subtle.generateKey(
		{
			name: 'RSA-OAEP',
			modulusLength: 2048,
			publicExponent: new Uint8Array([1, 0, 1]),
			hash: 'SHA-256',
		},
		true,
		['encrypt', 'decrypt'],
	);
	const pair2 = await crypto.subtle.generateKey(
		{
			name: 'RSA-OAEP',
			modulusLength: 2048,
			publicExponent: new Uint8Array([1, 0, 1]),
			hash: 'SHA-256',
		},
		true,
		['encrypt', 'decrypt'],
	);

	const encrypted = await crypto.subtle.encrypt(
		{ name: 'RSA-OAEP' },
		pair1.publicKey,
		new TextEncoder().encode('test'),
	);

	let failed = false;
	try {
		await crypto.subtle.decrypt(
			{ name: 'RSA-OAEP' },
			pair2.privateKey,
			encrypted,
		);
	} catch {
		failed = true;
	}
	t.ok(failed, 'wrong key causes decrypt failure');
});

test('RSA-OAEP key usages', async (t) => {
	const pair = await crypto.subtle.generateKey(
		{
			name: 'RSA-OAEP',
			modulusLength: 2048,
			publicExponent: new Uint8Array([1, 0, 1]),
			hash: 'SHA-256',
		},
		true,
		['encrypt', 'decrypt'],
	);

	t.deepEqual(
		Array.from(pair.publicKey.usages).sort(),
		['encrypt'],
		'public key usages',
	);
	t.deepEqual(
		Array.from(pair.privateKey.usages).sort(),
		['decrypt'],
		'private key usages',
	);
});
