// Test RSASSA-PKCS1-v1_5 sign/verify

async function run() {
	var results = {};

	// --- Generate RSASSA-PKCS1-v1_5 key pair ---
	var keyPair = await crypto.subtle.generateKey(
		{
			name: 'RSASSA-PKCS1-v1_5',
			modulusLength: 2048,
			publicExponent: new Uint8Array([1, 0, 1]),
			hash: 'SHA-256',
		},
		true,
		['sign', 'verify']
	);

	results.keyPairTypes = {
		publicType: keyPair.publicKey.type,
		privateType: keyPair.privateKey.type,
		publicAlgo: keyPair.publicKey.algorithm.name,
		privateAlgo: keyPair.privateKey.algorithm.name,
		publicHash: keyPair.publicKey.algorithm.hash.name,
		modulusLength: keyPair.publicKey.algorithm.modulusLength,
	};

	// --- Sign / Verify round trip ---
	var data = textEncode('Hello PKCS1!');
	var signature = await crypto.subtle.sign(
		{ name: 'RSASSA-PKCS1-v1_5' },
		keyPair.privateKey,
		data
	);

	results.signatureLength = signature.byteLength;

	var verified = await crypto.subtle.verify(
		{ name: 'RSASSA-PKCS1-v1_5' },
		keyPair.publicKey,
		signature,
		data
	);

	results.verified = verified;

	// --- Verify with wrong data should fail ---
	var wrongData = textEncode('Wrong data');
	var wrongVerified = await crypto.subtle.verify(
		{ name: 'RSASSA-PKCS1-v1_5' },
		keyPair.publicKey,
		signature,
		wrongData
	);

	results.wrongDataVerified = wrongVerified;

	// --- Key usages ---
	results.publicUsages = Array.prototype.slice.call(keyPair.publicKey.usages).sort();
	results.privateUsages = Array.prototype.slice.call(keyPair.privateKey.usages).sort();

	__output(results);
}

run();
