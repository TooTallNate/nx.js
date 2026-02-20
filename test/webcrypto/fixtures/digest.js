// Test crypto.subtle.digest() with all supported hash algorithms

function toHex(buf) {
	var bytes = new Uint8Array(buf);
	var hex = '';
	for (var i = 0; i < bytes.length; i++) {
		hex += (bytes[i] < 16 ? '0' : '') + bytes[i].toString(16);
	}
	return hex;
}

var testCases = [
	{ algo: 'SHA-1', input: '' },
	{ algo: 'SHA-1', input: 'abc' },
	{ algo: 'SHA-1', input: 'Hello, nx.js WebCrypto!' },
	{ algo: 'SHA-256', input: '' },
	{ algo: 'SHA-256', input: 'abc' },
	{ algo: 'SHA-256', input: 'Hello, nx.js WebCrypto!' },
	{ algo: 'SHA-384', input: '' },
	{ algo: 'SHA-384', input: 'abc' },
	{ algo: 'SHA-384', input: 'Hello, nx.js WebCrypto!' },
	{ algo: 'SHA-512', input: '' },
	{ algo: 'SHA-512', input: 'abc' },
	{ algo: 'SHA-512', input: 'Hello, nx.js WebCrypto!' },
];

async function run() {
	var results = {};
	for (var i = 0; i < testCases.length; i++) {
		var tc = testCases[i];
		var data = textEncode(tc.input);
		var digest = await crypto.subtle.digest(tc.algo, data);
		results[tc.algo + ':' + tc.input] = toHex(digest);
	}
	__output(results);
}

run();
