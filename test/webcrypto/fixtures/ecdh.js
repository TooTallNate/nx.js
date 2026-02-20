// Test ECDH key derivation

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

	// Generate two P-256 keypairs
	var alice = await crypto.subtle.generateKey(
		{ name: 'ECDH', namedCurve: 'P-256' },
		true,
		['deriveBits', 'deriveKey']
	);
	var bob = await crypto.subtle.generateKey(
		{ name: 'ECDH', namedCurve: 'P-256' },
		true,
		['deriveBits', 'deriveKey']
	);

	results.aliceKeyProps = {
		privateType: alice.privateKey.type,
		publicType: alice.publicKey.type,
		algoName: alice.privateKey.algorithm.name,
		namedCurve: alice.privateKey.algorithm.namedCurve,
	};

	// Derive shared secret from both sides
	var aliceShared = await crypto.subtle.deriveBits(
		{ name: 'ECDH', public: bob.publicKey },
		alice.privateKey,
		256
	);
	var bobShared = await crypto.subtle.deriveBits(
		{ name: 'ECDH', public: alice.publicKey },
		bob.privateKey,
		256
	);

	results.p256sharedLen = aliceShared.byteLength;
	results.p256sharedMatch = bufToHex(aliceShared) === bufToHex(bobShared);

	// Test P-384
	var alice384 = await crypto.subtle.generateKey(
		{ name: 'ECDH', namedCurve: 'P-384' },
		true,
		['deriveBits']
	);
	var bob384 = await crypto.subtle.generateKey(
		{ name: 'ECDH', namedCurve: 'P-384' },
		true,
		['deriveBits']
	);

	var alice384Shared = await crypto.subtle.deriveBits(
		{ name: 'ECDH', public: bob384.publicKey },
		alice384.privateKey,
		384
	);
	var bob384Shared = await crypto.subtle.deriveBits(
		{ name: 'ECDH', public: alice384.publicKey },
		bob384.privateKey,
		384
	);

	results.p384sharedLen = alice384Shared.byteLength;
	results.p384sharedMatch = bufToHex(alice384Shared) === bufToHex(bob384Shared);

	// Export and reimport public key, then derive
	var exportedBobPub = await crypto.subtle.exportKey('raw', bob.publicKey);
	results.pubExportLen = exportedBobPub.byteLength;

	var reimportedBobPub = await crypto.subtle.importKey(
		'raw',
		exportedBobPub,
		{ name: 'ECDH', namedCurve: 'P-256' },
		true,
		[]
	);
	var reimportShared = await crypto.subtle.deriveBits(
		{ name: 'ECDH', public: reimportedBobPub },
		alice.privateKey,
		256
	);
	results.reimportSharedMatch = bufToHex(reimportShared) === bufToHex(aliceShared);

	__output(results);
}

run();
