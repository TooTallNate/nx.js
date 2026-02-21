// Test AES-GCM encrypt/decrypt

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

	// --- AES-GCM-128 basic round trip ---
	var keyData128 = new Uint8Array(16);
	var iv = new Uint8Array(12); // standard 96-bit IV
	var plaintext = textEncode('Hello AES-GCM!');

	var key128 = await crypto.subtle.importKey(
		'raw', keyData128,
		{ name: 'AES-GCM' },
		true,
		['encrypt', 'decrypt']
	);

	var encrypted128 = await crypto.subtle.encrypt(
		{ name: 'AES-GCM', iv: iv },
		key128,
		plaintext
	);

	// Ciphertext should be plaintext.length + 16 (tag)
	var decrypted128 = await crypto.subtle.decrypt(
		{ name: 'AES-GCM', iv: iv },
		key128,
		encrypted128
	);

	results['128'] = {
		ciphertextLen: encrypted128.byteLength,
		expectedLen: plaintext.byteLength + 16,
		decrypted: textDecode(decrypted128),
		roundTrip: textDecode(decrypted128) === 'Hello AES-GCM!',
	};

	// --- AES-GCM-256 basic round trip ---
	var keyData256 = new Uint8Array(32);
	var key256 = await crypto.subtle.importKey(
		'raw', keyData256,
		{ name: 'AES-GCM' },
		true,
		['encrypt', 'decrypt']
	);

	var encrypted256 = await crypto.subtle.encrypt(
		{ name: 'AES-GCM', iv: iv },
		key256,
		plaintext
	);
	var decrypted256 = await crypto.subtle.decrypt(
		{ name: 'AES-GCM', iv: iv },
		key256,
		encrypted256
	);

	results['256'] = {
		ciphertextLen: encrypted256.byteLength,
		roundTrip: textDecode(decrypted256) === 'Hello AES-GCM!',
	};

	// --- With additionalData (AAD) ---
	var aad = textEncode('authenticated header');
	var encryptedAAD = await crypto.subtle.encrypt(
		{ name: 'AES-GCM', iv: iv, additionalData: aad },
		key128,
		plaintext
	);
	var decryptedAAD = await crypto.subtle.decrypt(
		{ name: 'AES-GCM', iv: iv, additionalData: aad },
		key128,
		encryptedAAD
	);

	results.aad = {
		roundTrip: textDecode(decryptedAAD) === 'Hello AES-GCM!',
	};

	// --- Decryption with wrong AAD should fail ---
	var wrongAAD = textEncode('wrong header');
	var aadFailed = false;
	try {
		await crypto.subtle.decrypt(
			{ name: 'AES-GCM', iv: iv, additionalData: wrongAAD },
			key128,
			encryptedAAD
		);
	} catch (e) {
		aadFailed = true;
	}
	results.wrongAAD = { failed: aadFailed };

	// --- Decryption with wrong key should fail ---
	var wrongKeyData = new Uint8Array(16);
	wrongKeyData[0] = 0xff;
	var wrongKey = await crypto.subtle.importKey(
		'raw', wrongKeyData,
		{ name: 'AES-GCM' },
		false,
		['decrypt']
	);
	var wrongKeyFailed = false;
	try {
		await crypto.subtle.decrypt(
			{ name: 'AES-GCM', iv: iv },
			wrongKey,
			encrypted128
		);
	} catch (e) {
		wrongKeyFailed = true;
	}
	results.wrongKey = { failed: wrongKeyFailed };

	// --- Custom tagLength (96 bits = 12 bytes) ---
	var encrypted96 = await crypto.subtle.encrypt(
		{ name: 'AES-GCM', iv: iv, tagLength: 96 },
		key128,
		plaintext
	);
	var decrypted96 = await crypto.subtle.decrypt(
		{ name: 'AES-GCM', iv: iv, tagLength: 96 },
		key128,
		encrypted96
	);
	results.tagLength96 = {
		ciphertextLen: encrypted96.byteLength,
		expectedLen: plaintext.byteLength + 12,
		roundTrip: textDecode(decrypted96) === 'Hello AES-GCM!',
	};

	// --- Key properties ---
	results.keyProps = {
		type: key128.type,
		algorithmName: key128.algorithm.name,
		algorithmLength: key128.algorithm.length,
		extractable: key128.extractable,
		usages: Array.prototype.slice.call(key128.usages).sort(),
	};

	// --- generateKey ---
	var genKey = await crypto.subtle.generateKey(
		{ name: 'AES-GCM', length: 256 },
		true,
		['encrypt', 'decrypt']
	);
	var encGen = await crypto.subtle.encrypt(
		{ name: 'AES-GCM', iv: iv },
		genKey,
		plaintext
	);
	var decGen = await crypto.subtle.decrypt(
		{ name: 'AES-GCM', iv: iv },
		genKey,
		encGen
	);
	results.generateKey = {
		roundTrip: textDecode(decGen) === 'Hello AES-GCM!',
		algorithmName: genKey.algorithm.name,
	};

	// --- exportKey round trip ---
	var exported = await crypto.subtle.exportKey('raw', key128);
	var reimported = await crypto.subtle.importKey(
		'raw', exported,
		{ name: 'AES-GCM' },
		true,
		['encrypt', 'decrypt']
	);
	var encReimported = await crypto.subtle.encrypt(
		{ name: 'AES-GCM', iv: iv },
		reimported,
		plaintext
	);
	// Should produce identical ciphertext since same key+iv+plaintext
	results.exportKey = {
		sameOutput: bufToHex(encReimported) === bufToHex(encrypted128),
	};

	__output(results);
}

run();
