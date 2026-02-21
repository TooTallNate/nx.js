// Test JWK import/export

function bufToHex(buf) {
	var bytes = new Uint8Array(buf);
	var hex = '';
	for (var i = 0; i < bytes.length; i++) {
		var b = bytes[i].toString(16);
		if (b.length < 2) b = '0' + b;
		hex += b;
	}
	return hex;
}

async function run() {
	var results = {};

	// --- AES-GCM JWK round trip ---
	var aesKey = await crypto.subtle.generateKey(
		{ name: 'AES-GCM', length: 256 },
		true,
		['encrypt', 'decrypt']
	);

	var aesJwk = await crypto.subtle.exportKey('jwk', aesKey);
	results.aesJwk = {
		kty: aesJwk.kty,
		hasK: typeof aesJwk.k === 'string',
		ext: aesJwk.ext,
	};

	var reimportedAes = await crypto.subtle.importKey(
		'jwk',
		aesJwk,
		{ name: 'AES-GCM' },
		true,
		['encrypt', 'decrypt']
	);

	// Verify by encrypting with both keys
	var iv = new Uint8Array(12);
	var plaintext = textEncode('JWK test');
	var enc1 = await crypto.subtle.encrypt({ name: 'AES-GCM', iv: iv }, aesKey, plaintext);
	var dec2 = await crypto.subtle.decrypt({ name: 'AES-GCM', iv: iv }, reimportedAes, enc1);
	results.aesRoundTrip = textDecode(dec2) === 'JWK test';

	// --- HMAC JWK round trip ---
	var hmacKey = await crypto.subtle.generateKey(
		{ name: 'HMAC', hash: 'SHA-256' },
		true,
		['sign', 'verify']
	);

	var hmacJwk = await crypto.subtle.exportKey('jwk', hmacKey);
	results.hmacJwk = {
		kty: hmacJwk.kty,
		hasK: typeof hmacJwk.k === 'string',
	};

	var reimportedHmac = await crypto.subtle.importKey(
		'jwk',
		hmacJwk,
		{ name: 'HMAC', hash: 'SHA-256' },
		true,
		['sign', 'verify']
	);

	var sig1 = await crypto.subtle.sign({ name: 'HMAC' }, hmacKey, plaintext);
	var ver2 = await crypto.subtle.verify({ name: 'HMAC' }, reimportedHmac, sig1, plaintext);
	results.hmacRoundTrip = ver2;

	// --- RSA-OAEP JWK round trip ---
	var rsaKeyPair = await crypto.subtle.generateKey(
		{
			name: 'RSA-OAEP',
			modulusLength: 2048,
			publicExponent: new Uint8Array([1, 0, 1]),
			hash: 'SHA-256',
		},
		true,
		['encrypt', 'decrypt']
	);

	var rsaPubJwk = await crypto.subtle.exportKey('jwk', rsaKeyPair.publicKey);
	results.rsaPubJwk = {
		kty: rsaPubJwk.kty,
		hasN: typeof rsaPubJwk.n === 'string',
		hasE: typeof rsaPubJwk.e === 'string',
		hasD: typeof rsaPubJwk.d === 'string',
	};

	var rsaPrivJwk = await crypto.subtle.exportKey('jwk', rsaKeyPair.privateKey);
	results.rsaPrivJwk = {
		kty: rsaPrivJwk.kty,
		hasN: typeof rsaPrivJwk.n === 'string',
		hasD: typeof rsaPrivJwk.d === 'string',
		hasP: typeof rsaPrivJwk.p === 'string',
		hasQ: typeof rsaPrivJwk.q === 'string',
		hasDp: typeof rsaPrivJwk.dp === 'string',
		hasDq: typeof rsaPrivJwk.dq === 'string',
		hasQi: typeof rsaPrivJwk.qi === 'string',
	};

	// Re-import public key and encrypt, decrypt with original private
	var reimportedPub = await crypto.subtle.importKey(
		'jwk',
		rsaPubJwk,
		{ name: 'RSA-OAEP', hash: 'SHA-256' },
		true,
		['encrypt']
	);

	var rsaEnc = await crypto.subtle.encrypt({ name: 'RSA-OAEP' }, reimportedPub, plaintext);
	var rsaDec = await crypto.subtle.decrypt({ name: 'RSA-OAEP' }, rsaKeyPair.privateKey, rsaEnc);
	results.rsaJwkRoundTrip = textDecode(rsaDec) === 'JWK test';

	// --- ECDSA JWK: public vs private key field checks (case 2) ---
	var ecKeyPair = await crypto.subtle.generateKey(
		{ name: 'ECDSA', namedCurve: 'P-256' },
		true,
		['sign', 'verify']
	);

	var ecPubJwk = await crypto.subtle.exportKey('jwk', ecKeyPair.publicKey);
	var ecPrivJwk = await crypto.subtle.exportKey('jwk', ecKeyPair.privateKey);

	// Public JWK should have x, y but NOT d
	results.ecPubHasX = typeof ecPubJwk.x === 'string' && ecPubJwk.x.length > 0;
	results.ecPubHasY = typeof ecPubJwk.y === 'string' && ecPubJwk.y.length > 0;
	results.ecPubNoD = typeof ecPubJwk.d === 'undefined';

	// Private JWK should have x, y AND d
	results.ecPrivHasX = typeof ecPrivJwk.x === 'string' && ecPrivJwk.x.length > 0;
	results.ecPrivHasY = typeof ecPrivJwk.y === 'string' && ecPrivJwk.y.length > 0;
	results.ecPrivHasD = typeof ecPrivJwk.d === 'string' && ecPrivJwk.d.length > 0;

	// x and y MUST match between public and private exports (same key pair)
	results.ecXyMatch = ecPubJwk.x === ecPrivJwk.x && ecPubJwk.y === ecPrivJwk.y;

	// --- ECDSA JWK: x/y are correct public point, not derived from d (case 1) ---
	// Verify that x/y from the private key JWK are actually the public point:
	// reimport private JWK, sign; verify with ORIGINAL public key (not reimported)
	var reimportedEcPriv = await crypto.subtle.importKey(
		'jwk',
		ecPrivJwk,
		{ name: 'ECDSA', namedCurve: 'P-256' },
		true,
		['sign']
	);

	var ecData = textEncode('EC JWK x/y verification');
	var ecSig = await crypto.subtle.sign(
		{ name: 'ECDSA', hash: 'SHA-256' },
		reimportedEcPriv,
		ecData
	);
	// Verify with the ORIGINAL public key — proves the reimported private key
	// has the correct key material (x/y were the real public point)
	var ecVerOriginal = await crypto.subtle.verify(
		{ name: 'ECDSA', hash: 'SHA-256' },
		ecKeyPair.publicKey,
		ecSig,
		ecData
	);
	results.ecJwkPrivSignVerifyWithOriginalPub = ecVerOriginal;

	// Also reimport the public JWK and verify
	var reimportedEcPub = await crypto.subtle.importKey(
		'jwk',
		ecPubJwk,
		{ name: 'ECDSA', namedCurve: 'P-256' },
		true,
		['verify']
	);
	var ecVerReimported = await crypto.subtle.verify(
		{ name: 'ECDSA', hash: 'SHA-256' },
		reimportedEcPub,
		ecSig,
		ecData
	);
	results.ecJwkRoundTrip = ecVerReimported;

	// Verify d is NOT the same bytes as the public point (the original bug)
	// d should be ~32 bytes for P-256, while x and y are each 32 bytes
	// If x/y were mistakenly derived from the raw private scalar, they'd be wrong
	results.ecDNotEqualX = ecPrivJwk.d !== ecPrivJwk.x;
	results.ecDNotEqualY = ecPrivJwk.d !== ecPrivJwk.y;

	__output(results);
}

run();
