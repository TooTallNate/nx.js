// Test AES-CBC encrypt/decrypt with known key and IV

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

	// Fixed 128-bit key (16 bytes of zeros)
	var keyData = new Uint8Array(16);
	var key = await crypto.subtle.importKey(
		'raw', keyData,
		{ name: 'AES-CBC' },
		true,
		['encrypt', 'decrypt']
	);

	// Fixed IV (16 bytes of zeros)
	var iv = new Uint8Array(16);

	// Encrypt "Hello, World!"
	var plaintext = textEncode('Hello, World!');
	var ciphertext = await crypto.subtle.encrypt(
		{ name: 'AES-CBC', iv: iv },
		key,
		plaintext
	);
	results['encrypt:128:hello'] = toHex(ciphertext);

	// Decrypt back
	var decrypted = await crypto.subtle.decrypt(
		{ name: 'AES-CBC', iv: iv },
		key,
		ciphertext
	);
	results['decrypt:128:hello'] = textDecode(decrypted);

	// 256-bit key
	var keyData256 = new Uint8Array(32);
	for (var i = 0; i < 32; i++) keyData256[i] = i;
	var key256 = await crypto.subtle.importKey(
		'raw', keyData256,
		{ name: 'AES-CBC' },
		true,
		['encrypt', 'decrypt']
	);

	var ciphertext256 = await crypto.subtle.encrypt(
		{ name: 'AES-CBC', iv: iv },
		key256,
		textEncode('AES-256 test')
	);
	results['encrypt:256:test'] = toHex(ciphertext256);

	var decrypted256 = await crypto.subtle.decrypt(
		{ name: 'AES-CBC', iv: iv },
		key256,
		ciphertext256
	);
	results['decrypt:256:test'] = textDecode(decrypted256);

	// Export key round-trip
	var exported = await crypto.subtle.exportKey('raw', key);
	results['exportKey:128:length'] = exported.byteLength;

	__output(results);
}

run();
