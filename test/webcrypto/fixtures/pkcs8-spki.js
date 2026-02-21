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

	// --- ECDSA PKCS8/SPKI round trip ---
	var ecKeyPair = await crypto.subtle.generateKey(
		{ name: 'ECDSA', namedCurve: 'P-256' },
		true,
		['sign', 'verify']
	);

	// Export SPKI (public)
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

	// Export PKCS8 (private)
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

	// Sign with reimported private, verify with reimported public
	var ecData = textEncode('EC PKCS8/SPKI test');
	var ecSignature = await crypto.subtle.sign(
		{ name: 'ECDSA', hash: 'SHA-256' },
		reimportedEcPriv,
		ecData
	);
	var ecVerified = await crypto.subtle.verify(
		{ name: 'ECDSA', hash: 'SHA-256' },
		reimportedEcPub,
		ecSignature,
		ecData
	);
	results.ecReimportRoundTripVerified = ecVerified;

	__output(results);
}

run();
