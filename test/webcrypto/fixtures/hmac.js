// Test HMAC sign/verify with known keys

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

	// Test with known zero key + "hello" for each hash algo
	var algos = ['SHA-1', 'SHA-256', 'SHA-384', 'SHA-512'];
	var keyData = new Uint8Array(32); // all zeros
	var data = textEncode('hello');

	for (var i = 0; i < algos.length; i++) {
		var algo = algos[i];
		var key = await crypto.subtle.importKey(
			'raw', keyData,
			{ name: 'HMAC', hash: { name: algo } },
			false,
			['sign', 'verify']
		);

		var sig = await crypto.subtle.sign('HMAC', key, data);
		var valid = await crypto.subtle.verify('HMAC', key, sig, data);
		var tampered = await crypto.subtle.verify(
			'HMAC', key, sig, textEncode('world')
		);

		results[algo] = {
			signature: bufToHex(sig),
			signatureLen: sig.byteLength,
			valid: valid,
			tampered: tampered,
		};
	}

	// Test key properties
	var testKey = await crypto.subtle.importKey(
		'raw', keyData,
		{ name: 'HMAC', hash: { name: 'SHA-256' } },
		true,
		['sign', 'verify']
	);
	results.keyProps = {
		type: testKey.type,
		extractable: testKey.extractable,
		algorithmName: testKey.algorithm.name,
		usages: Array.prototype.slice.call(testKey.usages).sort(),
	};

	// Test exportKey round-trip
	var exported = await crypto.subtle.exportKey('raw', testKey);
	results.exportLen = exported.byteLength;
	results.exportMatch = bufToHex(exported) === bufToHex(keyData.buffer);

	__output(results);
}

run();
