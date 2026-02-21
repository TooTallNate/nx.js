// Test RSA-OAEP encrypt/decrypt

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

	// --- Generate RSA-OAEP key pair ---
	var keyPair = await crypto.subtle.generateKey(
		{
			name: 'RSA-OAEP',
			modulusLength: 2048,
			publicExponent: new Uint8Array([1, 0, 1]),
			hash: 'SHA-256',
		},
		true,
		['encrypt', 'decrypt']
	);

	results.keyPairTypes = {
		publicType: keyPair.publicKey.type,
		privateType: keyPair.privateKey.type,
		publicAlgo: keyPair.publicKey.algorithm.name,
		privateAlgo: keyPair.privateKey.algorithm.name,
		publicHash: keyPair.publicKey.algorithm.hash.name,
		modulusLength: keyPair.publicKey.algorithm.modulusLength,
	};

	// --- Encrypt / Decrypt round trip ---
	var plaintext = textEncode('Hello RSA-OAEP!');
	var encrypted = await crypto.subtle.encrypt(
		{ name: 'RSA-OAEP' },
		keyPair.publicKey,
		plaintext
	);

	results.encryptedLength = encrypted.byteLength;

	var decrypted = await crypto.subtle.decrypt(
		{ name: 'RSA-OAEP' },
		keyPair.privateKey,
		encrypted
	);

	results.roundTrip = textDecode(decrypted) === 'Hello RSA-OAEP!';

	// --- Encrypt with wrong key should fail decrypt ---
	var keyPair2 = await crypto.subtle.generateKey(
		{
			name: 'RSA-OAEP',
			modulusLength: 2048,
			publicExponent: new Uint8Array([1, 0, 1]),
			hash: 'SHA-256',
		},
		true,
		['encrypt', 'decrypt']
	);

	var wrongKeyFailed = false;
	try {
		await crypto.subtle.decrypt(
			{ name: 'RSA-OAEP' },
			keyPair2.privateKey,
			encrypted
		);
	} catch (e) {
		wrongKeyFailed = true;
	}
	results.wrongKeyFailed = wrongKeyFailed;

	// --- Key usages ---
	results.publicUsages = Array.prototype.slice.call(keyPair.publicKey.usages).sort();
	results.privateUsages = Array.prototype.slice.call(keyPair.privateKey.usages).sort();

	__output(results);
}

run();
