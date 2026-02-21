// Test wrapKey/unwrapKey

async function run() {
	var results = {};

	// --- Wrap/Unwrap AES key with AES-GCM ---
	var wrappingKey = await crypto.subtle.generateKey(
		{ name: 'AES-GCM', length: 256 },
		true,
		['wrapKey', 'unwrapKey']
	);

	var keyToWrap = await crypto.subtle.generateKey(
		{ name: 'AES-GCM', length: 128 },
		true,
		['encrypt', 'decrypt']
	);

	var iv = new Uint8Array(12);
	crypto.getRandomValues(iv);

	var wrapped = await crypto.subtle.wrapKey(
		'raw',
		keyToWrap,
		wrappingKey,
		{ name: 'AES-GCM', iv: iv }
	);

	results.wrappedLength = wrapped.byteLength;
	results.wrappedIsArrayBuffer = wrapped instanceof ArrayBuffer;

	var unwrapped = await crypto.subtle.unwrapKey(
		'raw',
		wrapped,
		wrappingKey,
		{ name: 'AES-GCM', iv: iv },
		{ name: 'AES-GCM' },
		true,
		['encrypt', 'decrypt']
	);

	results.unwrappedType = unwrapped.type;
	results.unwrappedAlgo = unwrapped.algorithm.name;
	results.unwrappedExtractable = unwrapped.extractable;

	// Verify unwrapped key works: encrypt with original, decrypt with unwrapped
	var plaintext = textEncode('wrap/unwrap test');
	var testIv = new Uint8Array(12);
	var enc = await crypto.subtle.encrypt(
		{ name: 'AES-GCM', iv: testIv },
		keyToWrap,
		plaintext
	);
	var dec = await crypto.subtle.decrypt(
		{ name: 'AES-GCM', iv: testIv },
		unwrapped,
		enc
	);

	results.roundTrip = textDecode(dec) === 'wrap/unwrap test';

	__output(results);
}

run();
