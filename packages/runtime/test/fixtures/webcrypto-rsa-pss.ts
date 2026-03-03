import { test } from '../src/tap';

test('RSA-PSS sign/verify round trip', async (t) => {
	const pair = await crypto.subtle.generateKey(
		{
			name: 'RSA-PSS',
			modulusLength: 2048,
			publicExponent: new Uint8Array([1, 0, 1]),
			hash: 'SHA-256',
		},
		true,
		['sign', 'verify'],
	);

	t.equal(pair.publicKey.type, 'public', 'public key type');
	t.equal(pair.privateKey.type, 'private', 'private key type');
	t.equal(pair.publicKey.algorithm.name, 'RSA-PSS', 'public algo');
	t.equal(pair.privateKey.algorithm.name, 'RSA-PSS', 'private algo');
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

	const data = new TextEncoder().encode('Hello RSA-PSS!');
	const sig = await crypto.subtle.sign(
		{ name: 'RSA-PSS', saltLength: 32 },
		pair.privateKey,
		data,
	);

	t.equal(sig.byteLength, 256, 'signature is 256 bytes');

	const verified = await crypto.subtle.verify(
		{ name: 'RSA-PSS', saltLength: 32 },
		pair.publicKey,
		sig,
		data,
	);
	t.ok(verified, 'signature verifies');

	const wrongVerified = await crypto.subtle.verify(
		{ name: 'RSA-PSS', saltLength: 32 },
		pair.publicKey,
		sig,
		new TextEncoder().encode('Wrong data'),
	);
	t.notOk(wrongVerified, 'wrong data fails verification');
});

test('RSA-PSS key usages', async (t) => {
	const pair = await crypto.subtle.generateKey(
		{
			name: 'RSA-PSS',
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
