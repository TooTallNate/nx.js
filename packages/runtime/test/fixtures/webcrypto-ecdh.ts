import { test } from '../src/tap';

function toHex(buf: ArrayBuffer): string {
	const bytes = new Uint8Array(buf);
	let hex = '';
	for (let i = 0; i < bytes.length; i++) {
		hex += bytes[i].toString(16).padStart(2, '0');
	}
	return hex;
}

test('ECDH P-256 shared secret derivation', async (t) => {
	const alice = await crypto.subtle.generateKey(
		{ name: 'ECDH', namedCurve: 'P-256' },
		true,
		['deriveBits', 'deriveKey'],
	);
	const bob = await crypto.subtle.generateKey(
		{ name: 'ECDH', namedCurve: 'P-256' },
		true,
		['deriveBits', 'deriveKey'],
	);

	const aliceShared = await crypto.subtle.deriveBits(
		{ name: 'ECDH', public: bob.publicKey },
		alice.privateKey,
		256,
	);
	const bobShared = await crypto.subtle.deriveBits(
		{ name: 'ECDH', public: alice.publicKey },
		bob.privateKey,
		256,
	);

	t.equal(aliceShared.byteLength, 32, 'P-256 shared secret is 32 bytes');
	t.equal(toHex(aliceShared), toHex(bobShared), 'both sides derive same secret');
});

test('ECDH P-384 shared secret derivation', async (t) => {
	const alice = await crypto.subtle.generateKey(
		{ name: 'ECDH', namedCurve: 'P-384' },
		true,
		['deriveBits'],
	);
	const bob = await crypto.subtle.generateKey(
		{ name: 'ECDH', namedCurve: 'P-384' },
		true,
		['deriveBits'],
	);

	const aliceShared = await crypto.subtle.deriveBits(
		{ name: 'ECDH', public: bob.publicKey },
		alice.privateKey,
		384,
	);
	const bobShared = await crypto.subtle.deriveBits(
		{ name: 'ECDH', public: alice.publicKey },
		bob.privateKey,
		384,
	);

	t.equal(aliceShared.byteLength, 48, 'P-384 shared secret is 48 bytes');
	t.equal(toHex(aliceShared), toHex(bobShared), 'P-384 both sides match');
});

test('ECDH key properties', async (t) => {
	const pair = await crypto.subtle.generateKey(
		{ name: 'ECDH', namedCurve: 'P-256' },
		true,
		['deriveBits', 'deriveKey'],
	);

	t.equal(pair.privateKey.type, 'private', 'private key type');
	t.equal(pair.publicKey.type, 'public', 'public key type');
	t.equal(pair.privateKey.algorithm.name, 'ECDH', 'algorithm name');
	t.equal(
		(pair.privateKey.algorithm as EcKeyAlgorithm).namedCurve,
		'P-256',
		'named curve',
	);
});

test('ECDH export/reimport public key derivation', async (t) => {
	const alice = await crypto.subtle.generateKey(
		{ name: 'ECDH', namedCurve: 'P-256' },
		true,
		['deriveBits'],
	);
	const bob = await crypto.subtle.generateKey(
		{ name: 'ECDH', namedCurve: 'P-256' },
		true,
		['deriveBits'],
	);

	const aliceShared = await crypto.subtle.deriveBits(
		{ name: 'ECDH', public: bob.publicKey },
		alice.privateKey,
		256,
	);

	const exportedBob = await crypto.subtle.exportKey('raw', bob.publicKey);
	t.equal(exportedBob.byteLength, 65, 'P-256 uncompressed public key is 65 bytes');

	const reimportedBob = await crypto.subtle.importKey(
		'raw',
		exportedBob,
		{ name: 'ECDH', namedCurve: 'P-256' },
		true,
		[],
	);

	const reimportShared = await crypto.subtle.deriveBits(
		{ name: 'ECDH', public: reimportedBob },
		alice.privateKey,
		256,
	);
	t.equal(
		toHex(reimportShared),
		toHex(aliceShared),
		'reimported public key derives same secret',
	);
});
