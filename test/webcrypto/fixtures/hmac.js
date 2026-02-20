// Test HMAC sign/verify with known key material

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

	// Fixed key (32 bytes of zeros)
	var keyData = new Uint8Array(32);

	var hashAlgos = ['SHA-1', 'SHA-256', 'SHA-384', 'SHA-512'];

	for (var h = 0; h < hashAlgos.length; h++) {
		var hashAlgo = hashAlgos[h];
		var key = await crypto.subtle.importKey(
			'raw', keyData,
			{ name: 'HMAC', hash: { name: hashAlgo } },
			false,
			['sign', 'verify']
		);

		// Sign "hello"
		var data = textEncode('hello');
		var signature = await crypto.subtle.sign('HMAC', key, data);
		results['sign:' + hashAlgo + ':hello'] = toHex(signature);

		// Verify correct
		var valid = await crypto.subtle.verify('HMAC', key, signature, data);
		results['verify:' + hashAlgo + ':correct'] = valid;

		// Verify with tampered data
		var tampered = textEncode('world');
		var invalid = await crypto.subtle.verify('HMAC', key, signature, tampered);
		results['verify:' + hashAlgo + ':tampered'] = invalid;
	}

	// Test with non-zero key
	var key2Data = new Uint8Array(32);
	for (var i = 0; i < 32; i++) key2Data[i] = i;

	var key2 = await crypto.subtle.importKey(
		'raw', key2Data,
		{ name: 'HMAC', hash: { name: 'SHA-256' } },
		false,
		['sign', 'verify']
	);

	var sig2 = await crypto.subtle.sign('HMAC', key2, textEncode('test message'));
	results['sign:SHA-256:sequential-key'] = toHex(sig2);

	__output(results);
}

run();
