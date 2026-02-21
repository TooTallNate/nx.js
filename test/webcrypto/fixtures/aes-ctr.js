// Test AES-CTR encrypt/decrypt with known key and counter

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

	// 128-bit key
	var keyData = new Uint8Array(16);
	var counter = new Uint8Array(16);
	counter[15] = 1; // counter = 1
	var plaintext = textEncode('Hello AES-CTR!');

	var key = await crypto.subtle.importKey(
		'raw', keyData,
		{ name: 'AES-CTR' },
		true,
		['encrypt', 'decrypt']
	);

	var encrypted = await crypto.subtle.encrypt(
		{ name: 'AES-CTR', counter: counter, length: 128 },
		key,
		plaintext
	);
	var decrypted = await crypto.subtle.decrypt(
		{ name: 'AES-CTR', counter: counter, length: 128 },
		key,
		encrypted
	);

	results['128'] = {
		ciphertext: bufToHex(encrypted),
		ciphertextLen: encrypted.byteLength,
		plaintextLen: plaintext.byteLength,
		decrypted: textDecode(decrypted),
		roundTrip: textDecode(decrypted) === 'Hello AES-CTR!',
		sameSize: encrypted.byteLength === plaintext.byteLength,
	};

	// 256-bit key
	var keyData256 = new Uint8Array(32);
	var key256 = await crypto.subtle.importKey(
		'raw', keyData256,
		{ name: 'AES-CTR' },
		true,
		['encrypt', 'decrypt']
	);

	var encrypted256 = await crypto.subtle.encrypt(
		{ name: 'AES-CTR', counter: counter, length: 128 },
		key256,
		plaintext
	);
	var decrypted256 = await crypto.subtle.decrypt(
		{ name: 'AES-CTR', counter: counter, length: 128 },
		key256,
		encrypted256
	);

	results['256'] = {
		ciphertext: bufToHex(encrypted256),
		ciphertextLen: encrypted256.byteLength,
		decrypted: textDecode(decrypted256),
		roundTrip: textDecode(decrypted256) === 'Hello AES-CTR!',
	};

	results.keyProps = {
		type: key.type,
		algorithmName: key.algorithm.name,
		extractable: key.extractable,
		usages: Array.prototype.slice.call(key.usages).sort(),
	};

	__output(results);
}

run();
