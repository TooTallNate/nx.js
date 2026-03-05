import { test } from '../src/tap';

test('AES-GCM JWK round trip', async (t) => {
	const key = await crypto.subtle.generateKey(
		{ name: 'AES-GCM', length: 256 },
		true,
		['encrypt', 'decrypt'],
	);

	const jwk = await crypto.subtle.exportKey('jwk', key);
	t.equal(jwk.kty, 'oct', 'AES JWK kty is oct');
	t.equal(typeof jwk.k, 'string', 'AES JWK has k field');
	t.equal(jwk.ext, true, 'AES JWK ext is true');

	const reimported = await crypto.subtle.importKey(
		'jwk',
		jwk,
		{ name: 'AES-GCM' },
		true,
		['encrypt', 'decrypt'],
	);

	const iv = new Uint8Array(12);
	const plaintext = new TextEncoder().encode('JWK test');
	const enc = await crypto.subtle.encrypt(
		{ name: 'AES-GCM', iv },
		key,
		plaintext,
	);
	const dec = await crypto.subtle.decrypt(
		{ name: 'AES-GCM', iv },
		reimported,
		enc,
	);
	t.equal(
		new TextDecoder().decode(dec),
		'JWK test',
		'AES JWK round trip works',
	);
});

test('HMAC JWK round trip', async (t) => {
	const key = await crypto.subtle.generateKey(
		{ name: 'HMAC', hash: 'SHA-256' },
		true,
		['sign', 'verify'],
	);

	const jwk = await crypto.subtle.exportKey('jwk', key);
	t.equal(jwk.kty, 'oct', 'HMAC JWK kty is oct');
	t.equal(typeof jwk.k, 'string', 'HMAC JWK has k field');

	const reimported = await crypto.subtle.importKey(
		'jwk',
		jwk,
		{ name: 'HMAC', hash: 'SHA-256' },
		true,
		['sign', 'verify'],
	);

	const plaintext = new TextEncoder().encode('JWK test');
	const sig = await crypto.subtle.sign({ name: 'HMAC' }, key, plaintext);
	const valid = await crypto.subtle.verify(
		{ name: 'HMAC' },
		reimported,
		sig,
		plaintext,
	);
	t.ok(valid, 'HMAC JWK round trip works');
});

test('RSA-OAEP JWK public/private key fields', async (t) => {
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

	const pubJwk = await crypto.subtle.exportKey('jwk', pair.publicKey);
	t.equal(pubJwk.kty, 'RSA', 'RSA public JWK kty');
	t.equal(typeof pubJwk.n, 'string', 'has n');
	t.equal(typeof pubJwk.e, 'string', 'has e');
	t.equal(typeof pubJwk.d, 'undefined', 'public JWK has no d');

	const privJwk = await crypto.subtle.exportKey('jwk', pair.privateKey);
	t.equal(privJwk.kty, 'RSA', 'RSA private JWK kty');
	t.equal(typeof privJwk.n, 'string', 'has n');
	t.equal(typeof privJwk.d, 'string', 'has d');
	t.equal(typeof privJwk.p, 'string', 'has p');
	t.equal(typeof privJwk.q, 'string', 'has q');
	t.equal(typeof privJwk.dp, 'string', 'has dp');
	t.equal(typeof privJwk.dq, 'string', 'has dq');
	t.equal(typeof privJwk.qi, 'string', 'has qi');

	// Reimport public JWK, encrypt, decrypt with original private
	const reimportedPub = await crypto.subtle.importKey(
		'jwk',
		pubJwk,
		{ name: 'RSA-OAEP', hash: 'SHA-256' },
		true,
		['encrypt'],
	);
	const plaintext = new TextEncoder().encode('JWK test');
	const enc = await crypto.subtle.encrypt(
		{ name: 'RSA-OAEP' },
		reimportedPub,
		plaintext,
	);
	const dec = await crypto.subtle.decrypt(
		{ name: 'RSA-OAEP' },
		pair.privateKey,
		enc,
	);
	t.equal(new TextDecoder().decode(dec), 'JWK test', 'RSA JWK round trip');
});

test('ECDSA JWK public vs private key fields', async (t) => {
	const pair = await crypto.subtle.generateKey(
		{ name: 'ECDSA', namedCurve: 'P-256' },
		true,
		['sign', 'verify'],
	);

	const pubJwk = await crypto.subtle.exportKey('jwk', pair.publicKey);
	const privJwk = await crypto.subtle.exportKey('jwk', pair.privateKey);

	// Public has x, y but not d
	t.ok(typeof pubJwk.x === 'string' && pubJwk.x.length > 0, 'public has x');
	t.ok(typeof pubJwk.y === 'string' && pubJwk.y.length > 0, 'public has y');
	t.equal(typeof pubJwk.d, 'undefined', 'public has no d');

	// Private has x, y, d
	t.ok(typeof privJwk.x === 'string' && privJwk.x.length > 0, 'private has x');
	t.ok(typeof privJwk.y === 'string' && privJwk.y.length > 0, 'private has y');
	t.ok(typeof privJwk.d === 'string' && privJwk.d.length > 0, 'private has d');

	// x, y must match between pub and priv
	t.equal(pubJwk.x, privJwk.x, 'x matches');
	t.equal(pubJwk.y, privJwk.y, 'y matches');

	// d is distinct from x and y
	t.notEqual(privJwk.d, privJwk.x, 'd != x');
	t.notEqual(privJwk.d, privJwk.y, 'd != y');
});

test('ECDSA JWK reimport sign/verify', async (t) => {
	const pair = await crypto.subtle.generateKey(
		{ name: 'ECDSA', namedCurve: 'P-256' },
		true,
		['sign', 'verify'],
	);

	const privJwk = await crypto.subtle.exportKey('jwk', pair.privateKey);
	const pubJwk = await crypto.subtle.exportKey('jwk', pair.publicKey);

	const reimportedPriv = await crypto.subtle.importKey(
		'jwk',
		privJwk,
		{ name: 'ECDSA', namedCurve: 'P-256' },
		true,
		['sign'],
	);

	const data = new TextEncoder().encode('EC JWK verification');
	const sig = await crypto.subtle.sign(
		{ name: 'ECDSA', hash: 'SHA-256' },
		reimportedPriv,
		data,
	);

	// Verify with original public key
	const valid1 = await crypto.subtle.verify(
		{ name: 'ECDSA', hash: 'SHA-256' },
		pair.publicKey,
		sig,
		data,
	);
	t.ok(valid1, 'reimported private key sign verified by original public');

	// Verify with reimported public key
	const reimportedPub = await crypto.subtle.importKey(
		'jwk',
		pubJwk,
		{ name: 'ECDSA', namedCurve: 'P-256' },
		true,
		['verify'],
	);
	const valid2 = await crypto.subtle.verify(
		{ name: 'ECDSA', hash: 'SHA-256' },
		reimportedPub,
		sig,
		data,
	);
	t.ok(valid2, 'reimported public key also verifies');
});

test('JWK base64url encoding has no padding', async (t) => {
	const aesKey = await crypto.subtle.generateKey(
		{ name: 'AES-GCM', length: 256 },
		true,
		['encrypt', 'decrypt'],
	);
	const aesJwk = await crypto.subtle.exportKey('jwk', aesKey);

	const ecPair = await crypto.subtle.generateKey(
		{ name: 'ECDSA', namedCurve: 'P-256' },
		true,
		['sign', 'verify'],
	);
	const ecPubJwk = await crypto.subtle.exportKey('jwk', ecPair.publicKey);
	const ecPrivJwk = await crypto.subtle.exportKey('jwk', ecPair.privateKey);

	const fields = [
		aesJwk.k,
		ecPubJwk.x,
		ecPubJwk.y,
		ecPrivJwk.x,
		ecPrivJwk.y,
		ecPrivJwk.d,
	].filter((v): v is string => typeof v === 'string');

	const allUnpadded = fields.every((v) => !v.includes('='));
	t.ok(allUnpadded, 'all base64url fields have no padding');
});
