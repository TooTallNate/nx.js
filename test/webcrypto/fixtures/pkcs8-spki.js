// Test PKCS8/SPKI import/export

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

	// --- RSA PKCS8/SPKI round trip ---
	var rsaKeyPair = await crypto.subtle.generateKey(
		{
			name: 'RSASSA-PKCS1-v1_5',
			modulusLength: 2048,
			publicExponent: new Uint8Array([1, 0, 1]),
			hash: 'SHA-256',
		},
		true,
		['sign', 'verify']
	);

	// Export SPKI
	var spki = await crypto.subtle.exportKey('spki', rsaKeyPair.publicKey);
	results.spkiIsArrayBuffer = spki instanceof ArrayBuffer;
	results.spkiHasData = spki.byteLength > 0;

	// Re-import SPKI
	var reimportedPub = await crypto.subtle.importKey(
		'spki',
		spki,
		{ name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' },
		true,
		['verify']
	);
	results.reimportedPubType = reimportedPub.type;
	results.reimportedPubAlgo = reimportedPub.algorithm.name;

	// Export PKCS8
	var pkcs8 = await crypto.subtle.exportKey('pkcs8', rsaKeyPair.privateKey);
	results.pkcs8IsArrayBuffer = pkcs8 instanceof ArrayBuffer;
	results.pkcs8HasData = pkcs8.byteLength > 0;

	// Re-import PKCS8
	var reimportedPriv = await crypto.subtle.importKey(
		'pkcs8',
		pkcs8,
		{ name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' },
		true,
		['sign']
	);
	results.reimportedPrivType = reimportedPriv.type;
	results.reimportedPrivAlgo = reimportedPriv.algorithm.name;

	// Sign with reimported private key, verify with reimported public key
	var data = textEncode('PKCS8/SPKI test');
	var signature = await crypto.subtle.sign(
		{ name: 'RSASSA-PKCS1-v1_5' },
		reimportedPriv,
		data
	);
	// Verify with the reimported public key (from same key pair)
	var verified = await crypto.subtle.verify(
		{ name: 'RSASSA-PKCS1-v1_5' },
		reimportedPub,
		signature,
		data
	);
	results.reimportRoundTripVerified = verified;

	// --- RSA SPKI cross-verification (case 5) ---
	// Sign with ORIGINAL private key, verify with SPKI-REIMPORTED public key.
	// This catches the mbedtls_rsa_copy bug where manual MPI export/import
	// lost internal RSA state needed for verify operations.
	var crossData = textEncode('RSA SPKI cross-verify test');
	var crossSig = await crypto.subtle.sign(
		{ name: 'RSASSA-PKCS1-v1_5' },
		rsaKeyPair.privateKey,
		crossData
	);
	var crossVerified = await crypto.subtle.verify(
		{ name: 'RSASSA-PKCS1-v1_5' },
		reimportedPub,
		crossSig,
		crossData
	);
	results.rsaSpkiCrossVerified = crossVerified;

	// --- ECDSA PKCS8/SPKI cross-verification (case 4) ---
	// Export EC private as PKCS8, reimport, sign.
	// Export EC public as SPKI, reimport, verify the signature.
	var ecKeyPair = await crypto.subtle.generateKey(
		{ name: 'ECDSA', namedCurve: 'P-256' },
		true,
		['sign', 'verify']
	);

	// Export and reimport PKCS8 (private)
	var ecPkcs8 = await crypto.subtle.exportKey('pkcs8', ecKeyPair.privateKey);
	results.ecPkcs8IsArrayBuffer = ecPkcs8 instanceof ArrayBuffer;
	results.ecPkcs8HasData = ecPkcs8.byteLength > 0;

	var reimportedEcPriv = await crypto.subtle.importKey(
		'pkcs8',
		ecPkcs8,
		{ name: 'ECDSA', namedCurve: 'P-256' },
		true,
		['sign']
	);
	results.ecReimportedPrivType = reimportedEcPriv.type;
	results.ecReimportedPrivAlgo = reimportedEcPriv.algorithm.name;

	// Export and reimport SPKI (public)
	var ecSpki = await crypto.subtle.exportKey('spki', ecKeyPair.publicKey);
	results.ecSpkiIsArrayBuffer = ecSpki instanceof ArrayBuffer;
	results.ecSpkiHasData = ecSpki.byteLength > 0;

	var reimportedEcPub = await crypto.subtle.importKey(
		'spki',
		ecSpki,
		{ name: 'ECDSA', namedCurve: 'P-256' },
		true,
		['verify']
	);
	results.ecReimportedPubType = reimportedEcPub.type;
	results.ecReimportedPubAlgo = reimportedEcPub.algorithm.name;

	// Sign with REIMPORTED private key (from PKCS8)
	var ecData = textEncode('EC PKCS8/SPKI cross-verify');
	var ecSignature = await crypto.subtle.sign(
		{ name: 'ECDSA', hash: 'SHA-256' },
		reimportedEcPriv,
		ecData
	);

	// Verify with REIMPORTED public key (from SPKI) — cross-verification
	var ecVerReimported = await crypto.subtle.verify(
		{ name: 'ECDSA', hash: 'SHA-256' },
		reimportedEcPub,
		ecSignature,
		ecData
	);
	results.ecCrossVerifyReimported = ecVerReimported;

	// Also verify with ORIGINAL public key to prove key material preserved
	var ecVerOriginal = await crypto.subtle.verify(
		{ name: 'ECDSA', hash: 'SHA-256' },
		ecKeyPair.publicKey,
		ecSignature,
		ecData
	);
	results.ecCrossVerifyOriginal = ecVerOriginal;

	// --- RSA PKCS1 sign with reimported SPKI public key (case 5) ---
	// This tests the mbedtls_rsa_copy path in SPKI import
	var rsaPkcs1Pair = await crypto.subtle.generateKey(
		{
			name: 'RSASSA-PKCS1-v1_5',
			modulusLength: 2048,
			publicExponent: new Uint8Array([1, 0, 1]),
			hash: 'SHA-256',
		},
		true,
		['sign', 'verify']
	);

	// Sign with ORIGINAL private key
	var rsaPkcs1Data = textEncode('RSA PKCS1 SPKI reimport test');
	var rsaPkcs1Sig = await crypto.subtle.sign(
		{ name: 'RSASSA-PKCS1-v1_5' },
		rsaPkcs1Pair.privateKey,
		rsaPkcs1Data
	);

	// Export public key as SPKI, reimport
	var rsaPkcs1Spki = await crypto.subtle.exportKey('spki', rsaPkcs1Pair.publicKey);
	var reimportedRsaPub = await crypto.subtle.importKey(
		'spki',
		rsaPkcs1Spki,
		{ name: 'RSASSA-PKCS1-v1_5', hash: 'SHA-256' },
		true,
		['verify']
	);

	// Verify with REIMPORTED public key — this is the specific bug path
	var rsaPkcs1Ver = await crypto.subtle.verify(
		{ name: 'RSASSA-PKCS1-v1_5' },
		reimportedRsaPub,
		rsaPkcs1Sig,
		rsaPkcs1Data
	);
	results.rsaPkcs1SpkiReimportVerify = rsaPkcs1Ver;

	// Also verify the reimported key's algorithm is correct
	results.rsaPkcs1ReimportAlgo = reimportedRsaPub.algorithm.name;
	results.rsaPkcs1ReimportHash = reimportedRsaPub.algorithm.hash.name;

	__output(results);
}

run();
