// Test AES-CTR encrypt/decrypt with known key and counter

function toHex(buf) {
	var bytes = new Uint8Array(buf);
	var hex = '';
	for (var i = 0; i < bytes.length; i++) {
		hex += (bytes[i] < 16 ? '0' : '') + bytes[i].toString(16);
	}
	return hex;
}

async function run() {
	var results = {};

	// Fixed 128-bit key
	var keyData = new Uint8Array(16);
	var key = await crypto.subtle.importKey(
		'raw', keyData,
		{ name: 'AES-CTR' },
		true,
		['encrypt', 'decrypt']
	);

	// Fixed counter (16 bytes of zeros)
	var counter = new Uint8Array(16);

	// Encrypt "Hello, World!"
	var plaintext = textEncode('Hello, World!');
	var ciphertext = await crypto.subtle.encrypt(
		{ name: 'AES-CTR', counter: counter, length: 64 },
		key,
		plaintext
	);
	results['encrypt:128:hello'] = toHex(ciphertext);

	// Decrypt back
	var decrypted = await crypto.subtle.decrypt(
		{ name: 'AES-CTR', counter: counter, length: 64 },
		key,
		ciphertext
	);
	results['decrypt:128:hello'] = textDecode(decrypted);

	// 256-bit key
	var keyData256 = new Uint8Array(32);
	for (var i = 0; i < 32; i++) keyData256[i] = i;
	var key256 = await crypto.subtle.importKey(
		'raw', keyData256,
		{ name: 'AES-CTR' },
		true,
		['encrypt', 'decrypt']
	);

	var ciphertext256 = await crypto.subtle.encrypt(
		{ name: 'AES-CTR', counter: counter, length: 64 },
		key256,
		textEncode('AES-256 test')
	);
	results['encrypt:256:test'] = toHex(ciphertext256);

	var decrypted256 = await crypto.subtle.decrypt(
		{ name: 'AES-CTR', counter: counter, length: 64 },
		key256,
		ciphertext256
	);
	results['decrypt:256:test'] = textDecode(decrypted256);

	__output(results);
}

run();
