import { test } from '../src/tap';

test('RSA SPKI/PKCS8 export and reimport round trip', async (t) => {
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

	// Export SPKI
	const spki = await crypto.subtle.exportKey('spki', pair.publicKey);
	t.ok(spki instanceof ArrayBuffer, 'SPKI is ArrayBuffer');
	t.ok(spki.byteLength > 0, 'SPKI has data');

	const reimportedPub = await crypto.subtle.importKey(
		'spki',
		spki,
		{ name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' },
		true,
		['verify'],
	);
	t.equal(reimportedPub.type, 'public', 'reimported pub type');
	t.equal(reimportedPub.algorithm.name, 'RSASSA-PKCS1-v1_5', 'reimported pub algo');

	// Export PKCS8
	const pkcs8 = await crypto.subtle.exportKey('pkcs8', pair.privateKey);
	t.ok(pkcs8 instanceof ArrayBuffer, 'PKCS8 is ArrayBuffer');
	t.ok(pkcs8.byteLength > 0, 'PKCS8 has data');

	const reimportedPriv = await crypto.subtle.importKey(
		'pkcs8',
		pkcs8,
		{ name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' },
		true,
		['sign'],
	);
	t.equal(reimportedPriv.type, 'private', 'reimported priv type');
	t.equal(reimportedPriv.algorithm.name, 'RSASSA-PKCS1-v1_5', 'reimported priv algo');

	// Sign with reimported private, verify with reimported public
	const data = new TextEncoder().encode('PKCS8/SPKI test');
	const sig = await crypto.subtle.sign(
		{ name: 'RSASSA-PKCS1-v1_5' },
		reimportedPriv,
		data,
	);
	const verified = await crypto.subtle.verify(
		{ name: 'RSASSA-PKCS1-v1_5' },
		reimportedPub,
		sig,
		data,
	);
	t.ok(verified, 'reimport round trip sign/verify works');
});

test('RSA SPKI cross-verification (original priv → reimported pub)', async (t) => {
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

	const data = new TextEncoder().encode('RSA SPKI cross-verify test');
	const sig = await crypto.subtle.sign(
		{ name: 'RSASSA-PKCS1-v1_5' },
		pair.privateKey,
		data,
	);

	const spki = await crypto.subtle.exportKey('spki', pair.publicKey);
	const reimportedPub = await crypto.subtle.importKey(
		'spki',
		spki,
		{ name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' },
		true,
		['verify'],
	);

	const verified = await crypto.subtle.verify(
		{ name: 'RSASSA-PKCS1-v1_5' },
		reimportedPub,
		sig,
		data,
	);
	t.ok(verified, 'SPKI reimported public key verifies original signature');
	t.equal(reimportedPub.algorithm.name, 'RSASSA-PKCS1-v1_5', 'algorithm name');
	t.equal(
		(reimportedPub.algorithm as RsaHashedKeyAlgorithm).hash.name,
		'SHA-256',
		'hash name',
	);
});

test('ECDSA PKCS8/SPKI cross-verification', async (t) => {
	const pair = await crypto.subtle.generateKey(
		{ name: 'ECDSA', namedCurve: 'P-256' },
		true,
		['sign', 'verify'],
	);

	// Export and reimport both keys
	const ecPkcs8 = await crypto.subtle.exportKey('pkcs8', pair.privateKey);
	t.ok(ecPkcs8 instanceof ArrayBuffer && ecPkcs8.byteLength > 0, 'EC PKCS8 has data');

	const reimportedPriv = await crypto.subtle.importKey(
		'pkcs8',
		ecPkcs8,
		{ name: 'ECDSA', namedCurve: 'P-256' },
		true,
		['sign'],
	);
	t.equal(reimportedPriv.type, 'private', 'EC reimported priv type');
	t.equal(reimportedPriv.algorithm.name, 'ECDSA', 'EC reimported priv algo');

	const ecSpki = await crypto.subtle.exportKey('spki', pair.publicKey);
	t.ok(ecSpki instanceof ArrayBuffer && ecSpki.byteLength > 0, 'EC SPKI has data');

	const reimportedPub = await crypto.subtle.importKey(
		'spki',
		ecSpki,
		{ name: 'ECDSA', namedCurve: 'P-256' },
		true,
		['verify'],
	);
	t.equal(reimportedPub.type, 'public', 'EC reimported pub type');

	// Sign with reimported private, verify with both
	const data = new TextEncoder().encode('EC PKCS8/SPKI cross-verify');
	const sig = await crypto.subtle.sign(
		{ name: 'ECDSA', hash: 'SHA-256' },
		reimportedPriv,
		data,
	);

	const v1 = await crypto.subtle.verify(
		{ name: 'ECDSA', hash: 'SHA-256' },
		reimportedPub,
		sig,
		data,
	);
	t.ok(v1, 'reimported pub verifies reimported priv signature');

	const v2 = await crypto.subtle.verify(
		{ name: 'ECDSA', hash: 'SHA-256' },
		pair.publicKey,
		sig,
		data,
	);
	t.ok(v2, 'original pub verifies reimported priv signature');
});
