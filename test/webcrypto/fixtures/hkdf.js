// Test HKDF deriveBits and deriveKey
// Test vectors from RFC 5869

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

function hexToBytes(hex) {
	var bytes = new Uint8Array(hex.length / 2);
	for (var i = 0; i < hex.length; i += 2) {
		bytes[i / 2] = parseInt(hex.substr(i, 2), 16);
	}
	return bytes;
}

async function run() {
	var results = {};

	// RFC 5869 Test Case 1
	// Hash = SHA-256
	// IKM  = 0x0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b (22 octets)
	// salt = 0x000102030405060708090a0b0c (13 octets)
	// info = 0xf0f1f2f3f4f5f6f7f8f9 (10 octets)
	// L    = 42
	// OKM  = 0x3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865

	var ikm1 = hexToBytes('0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b');
	var salt1 = hexToBytes('000102030405060708090a0b0c');
	var info1 = hexToBytes('f0f1f2f3f4f5f6f7f8f9');

	var key1 = await crypto.subtle.importKey(
		'raw', ikm1,
		{ name: 'HKDF' },
		false,
		['deriveBits', 'deriveKey']
	);

	results.keyProps = {
		type: key1.type,
		extractable: key1.extractable,
		algorithmName: key1.algorithm.name,
		usages: Array.prototype.slice.call(key1.usages).sort(),
	};

	var derived1 = await crypto.subtle.deriveBits(
		{ name: 'HKDF', hash: 'SHA-256', salt: salt1, info: info1 },
		key1, 336 // 42 bytes = 336 bits
	);
	results.rfc5869_tc1 = {
		hex: bufToHex(derived1),
		expected: '3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865',
		length: derived1.byteLength,
	};
	results.rfc5869_tc1.match = results.rfc5869_tc1.hex === results.rfc5869_tc1.expected;

	// RFC 5869 Test Case 3 - zero-length salt and info
	// Hash = SHA-256
	// IKM  = 0x0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b (22 octets)
	// salt = (empty)
	// info = (empty)
	// L    = 42
	// OKM  = 0x8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d9d201395faa4b61a96c8

	var ikm3 = hexToBytes('0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b');
	var key3 = await crypto.subtle.importKey(
		'raw', ikm3,
		{ name: 'HKDF' },
		false,
		['deriveBits', 'deriveKey']
	);

	var derived3 = await crypto.subtle.deriveBits(
		{ name: 'HKDF', hash: 'SHA-256', salt: new Uint8Array(0), info: new Uint8Array(0) },
		key3, 336
	);
	results.rfc5869_tc3 = {
		hex: bufToHex(derived3),
		expected: '8da4e775a563c18f715f802a063c5a31b8a11f5c5ee1879ec3454e5f3c738d2d9d201395faa4b61a96c8',
	};
	results.rfc5869_tc3.match = results.rfc5869_tc3.hex === results.rfc5869_tc3.expected;

	// Test deriveKey to AES-CBC
	var aesKey = await crypto.subtle.deriveKey(
		{ name: 'HKDF', hash: 'SHA-256', salt: salt1, info: info1 },
		key1,
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

	// Verify derived AES key works for encrypt/decrypt
	var iv = new Uint8Array(16);
	var plaintext = textEncode('Hello, HKDF!');
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
		match: decryptedText === 'Hello, HKDF!',
	};

	__output(results);
}

run();
