import { test } from '../src/tap';

test('RSASSA-PKCS1-v1_5 sign/verify round trip', async (t) => {
	const pair = await crypto.subtle.generateKey(
		{
			name: 'RSASSA-PKCS1-v1_5',
			modulusLength: 2048,
			publicExponent: new Uint8Array([1, 0, 1]),
			hash: 'SHA-256',
		},
		true,
		['sign', 'verify'],
	);

	t.equal(pair.publicKey.type, 'public', 'public key type');
	t.equal(pair.privateKey.type, 'private', 'private key type');
	t.equal(pair.publicKey.algorithm.name, 'RSASSA-PKCS1-v1_5', 'public algo');
	t.equal(pair.privateKey.algorithm.name, 'RSASSA-PKCS1-v1_5', 'private algo');
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

	const data = new TextEncoder().encode('Hello PKCS1!');
	const sig = await crypto.subtle.sign(
		{ name: 'RSASSA-PKCS1-v1_5' },
		pair.privateKey,
		data,
	);

	t.equal(sig.byteLength, 256, 'signature is 256 bytes');

	const verified = await crypto.subtle.verify(
		{ name: 'RSASSA-PKCS1-v1_5' },
		pair.publicKey,
		sig,
		data,
	);
	t.ok(verified, 'signature verifies');

	const wrongVerified = await crypto.subtle.verify(
		{ name: 'RSASSA-PKCS1-v1_5' },
		pair.publicKey,
		sig,
		new TextEncoder().encode('Wrong data'),
	);
	t.notOk(wrongVerified, 'wrong data fails verification');
});

test('RSASSA-PKCS1-v1_5 key usages', async (t) => {
	const pair = await crypto.subtle.generateKey(
		{
			name: 'RSASSA-PKCS1-v1_5',
			modulusLength: 2048,
			publicExponent: new Uint8Array([1, 0, 1]),
			hash: 'SHA-256',
		},
		true,
		['sign', 'verify'],
	);

	t.deepEqual(
		Array.from(pair.publicKey.usages).sort(),
		['verify'],
		'public key usages',
	);
	t.deepEqual(
		Array.from(pair.privateKey.usages).sort(),
		['sign'],
		'private key usages',
	);
});
