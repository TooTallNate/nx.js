// Test ECDSA sign/verify

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

	// Test P-256 with SHA-256
	var p256pair = await crypto.subtle.generateKey(
		{ name: 'ECDSA', namedCurve: 'P-256' },
		true,
		['sign', 'verify']
	);

	results.p256keyProps = {
		privateType: p256pair.privateKey.type,
		publicType: p256pair.publicKey.type,
		privateAlgoName: p256pair.privateKey.algorithm.name,
		publicAlgoName: p256pair.publicKey.algorithm.name,
		privateExtractable: p256pair.privateKey.extractable,
		publicExtractable: p256pair.publicKey.extractable,
	};

	// Sign and verify with P-256
	var data = textEncode('hello world');
	var sig256 = await crypto.subtle.sign(
		{ name: 'ECDSA', hash: { name: 'SHA-256' } },
		p256pair.privateKey,
		data
	);
	results.p256sigLen = sig256.byteLength;  // Should be 64 (32+32)

	var valid256 = await crypto.subtle.verify(
		{ name: 'ECDSA', hash: { name: 'SHA-256' } },
		p256pair.publicKey,
		sig256,
		data
	);
	results.p256valid = valid256;

	// Verify with wrong data should fail
	var invalid256 = await crypto.subtle.verify(
		{ name: 'ECDSA', hash: { name: 'SHA-256' } },
		p256pair.publicKey,
		sig256,
		textEncode('wrong data')
	);
	results.p256invalid = invalid256;

	// Export public key and reimport, then verify
	var exportedPub = await crypto.subtle.exportKey('raw', p256pair.publicKey);
	results.p256pubExportLen = exportedPub.byteLength;  // Should be 65 (1 + 32 + 32)
	results.p256pubExportPrefix = new Uint8Array(exportedPub)[0];  // Should be 0x04

	var reimportedPub = await crypto.subtle.importKey(
		'raw',
		exportedPub,
		{ name: 'ECDSA', namedCurve: 'P-256' },
		true,
		['verify']
	);
	var validReimport = await crypto.subtle.verify(
		{ name: 'ECDSA', hash: { name: 'SHA-256' } },
		reimportedPub,
		sig256,
		data
	);
	results.p256reimportVerify = validReimport;

	// Test P-384 with SHA-384
	var p384pair = await crypto.subtle.generateKey(
		{ name: 'ECDSA', namedCurve: 'P-384' },
		true,
		['sign', 'verify']
	);
	var sig384 = await crypto.subtle.sign(
		{ name: 'ECDSA', hash: { name: 'SHA-384' } },
		p384pair.privateKey,
		data
	);
	results.p384sigLen = sig384.byteLength;  // Should be 96 (48+48)

	var valid384 = await crypto.subtle.verify(
		{ name: 'ECDSA', hash: { name: 'SHA-384' } },
		p384pair.publicKey,
		sig384,
		data
	);
	results.p384valid = valid384;

	__output(results);
}

run();
