// Test AES-CBC encrypt/decrypt with known key and IV

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

	// 128-bit key (all zeros)
	var keyData128 = new Uint8Array(16);
	var iv = new Uint8Array(16); // all zeros
	var plaintext = textEncode('Hello AES-CBC!  '); // exactly 16 bytes

	var key128 = await crypto.subtle.importKey(
		'raw', keyData128,
		{ name: 'AES-CBC' },
		true,
		['encrypt', 'decrypt']
	);

	var encrypted128 = await crypto.subtle.encrypt(
		{ name: 'AES-CBC', iv: iv },
		key128,
		plaintext
	);
	var decrypted128 = await crypto.subtle.decrypt(
		{ name: 'AES-CBC', iv: iv },
		key128,
		encrypted128
	);

	results['128'] = {
		ciphertext: bufToHex(encrypted128),
		ciphertextLen: encrypted128.byteLength,
		decrypted: textDecode(decrypted128),
		roundTrip: textDecode(decrypted128) === 'Hello AES-CBC!  ',
	};

	// 256-bit key (all zeros)
	var keyData256 = new Uint8Array(32);
	var key256 = await crypto.subtle.importKey(
		'raw', keyData256,
		{ name: 'AES-CBC' },
		true,
		['encrypt', 'decrypt']
	);

	var encrypted256 = await crypto.subtle.encrypt(
		{ name: 'AES-CBC', iv: iv },
		key256,
		plaintext
	);
	var decrypted256 = await crypto.subtle.decrypt(
		{ name: 'AES-CBC', iv: iv },
		key256,
		encrypted256
	);

	results['256'] = {
		ciphertext: bufToHex(encrypted256),
		ciphertextLen: encrypted256.byteLength,
		decrypted: textDecode(decrypted256),
		roundTrip: textDecode(decrypted256) === 'Hello AES-CBC!  ',
	};

	// Key properties
	results.keyProps = {
		type: key128.type,
		algorithmName: key128.algorithm.name,
		extractable: key128.extractable,
		usages: Array.prototype.slice.call(key128.usages).sort(),
	};

	__output(results);
}

run();
