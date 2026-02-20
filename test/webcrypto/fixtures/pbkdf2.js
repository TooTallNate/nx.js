// Test PBKDF2 deriveBits and deriveKey

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

	// Import a password as PBKDF2 key
	var password = textEncode('password');
	var key = await crypto.subtle.importKey(
		'raw', password,
		{ name: 'PBKDF2' },
		false,
		['deriveBits', 'deriveKey']
	);

	results.keyProps = {
		type: key.type,
		extractable: key.extractable,
		algorithmName: key.algorithm.name,
		usages: Array.prototype.slice.call(key.usages).sort(),
	};

	// Test deriveBits with known vector
	// RFC 6070 test vector: password="password", salt="salt", iterations=4096, dkLen=20, SHA-1
	var salt1 = textEncode('salt');
	var derived1 = await crypto.subtle.deriveBits(
		{ name: 'PBKDF2', salt: salt1, iterations: 4096, hash: 'SHA-1' },
		key, 160
	);
	results.rfc6070 = {
		hex: bufToHex(derived1),
		// Expected: 4b007901b765489abead49d926f721d065a429c1
		expected: '4b007901b765489abead49d926f721d065a429c1',
	};
	results.rfc6070.match = results.rfc6070.hex === results.rfc6070.expected;

	// Test deriveBits with SHA-256
	var salt2 = textEncode('salt');
	var derived2 = await crypto.subtle.deriveBits(
		{ name: 'PBKDF2', salt: salt2, iterations: 1, hash: 'SHA-256' },
		key, 256
	);
	results.sha256 = {
		hex: bufToHex(derived2),
		length: derived2.byteLength,
		// Known: PBKDF2-SHA256("password", "salt", 1, 32)
		expected: '120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b',
	};
	results.sha256.match = results.sha256.hex === results.sha256.expected;

	// Test deriveKey to get an AES-CBC key, then encrypt/decrypt
	var salt3 = textEncode('mysalt');
	var aesKey = await crypto.subtle.deriveKey(
		{ name: 'PBKDF2', salt: salt3, iterations: 1000, hash: 'SHA-256' },
		key,
		{ name: 'AES-CBC', length: 256 },
		true,
		['encrypt', 'decrypt']
	);
	results.deriveKey = {
		type: aesKey.type,
		algorithmName: aesKey.algorithm.name,
		extractable: aesKey.extractable,
		keyLength: aesKey.algorithm.length,
	};

	// Encrypt then decrypt with derived key
	var iv = new Uint8Array(16);
	var plaintext = textEncode('Hello, PBKDF2!');
	var ciphertext = await crypto.subtle.encrypt(
		{ name: 'AES-CBC', iv: iv },
		aesKey, plaintext
	);
	var decrypted = await crypto.subtle.decrypt(
		{ name: 'AES-CBC', iv: iv },
		aesKey, ciphertext
	);
	var decryptedText = textDecode(new Uint8Array(decrypted));
	results.roundTrip = {
		originalLen: plaintext.byteLength,
		ciphertextLen: ciphertext.byteLength,
		decryptedText: decryptedText,
		match: decryptedText === 'Hello, PBKDF2!',
	};

	__output(results);
}

run();
