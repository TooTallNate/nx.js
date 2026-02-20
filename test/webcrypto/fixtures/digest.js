// Test crypto.subtle.digest() for SHA-1, SHA-256, SHA-384, SHA-512

var results = {};

// Helper: ArrayBuffer to hex string
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

var algos = ['SHA-1', 'SHA-256', 'SHA-384', 'SHA-512'];

// Test 1: empty input
var emptyData = new Uint8Array(0);

// Test 2: "abc"
var abcData = textEncode('abc');

// Test 3: longer string
var helloData = textEncode('Hello, nx.js WebCrypto!');

async function run() {
	for (var i = 0; i < algos.length; i++) {
		var algo = algos[i];
		var emptyHash = await crypto.subtle.digest(algo, emptyData);
		var abcHash = await crypto.subtle.digest(algo, abcData);
		var helloHash = await crypto.subtle.digest(algo, helloData);

		results[algo] = {
			empty: bufToHex(emptyHash),
			abc: bufToHex(abcHash),
			hello: bufToHex(helloHash),
			emptyLen: emptyHash.byteLength,
			abcLen: abcHash.byteLength,
		};
	}
	__output(results);
}

run();
