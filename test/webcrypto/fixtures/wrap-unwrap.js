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

	// --- wrapKey/unwrapKey with ONLY wrapKey/unwrapKey usages (case 3) ---
	// Verify that a key with ['wrapKey', 'unwrapKey'] (NOT ['encrypt', 'decrypt'])
	// can wrap and unwrap. This tests the usage check fix that accepts
	// NX_CRYPTO_KEY_USAGE_WRAP_KEY for encrypt and NX_CRYPTO_KEY_USAGE_UNWRAP_KEY for decrypt.
	var wrapOnlyKey = await crypto.subtle.generateKey(
		{ name: 'AES-GCM', length: 256 },
		true,
		['wrapKey', 'unwrapKey']
	);

	// Confirm this key does NOT have encrypt/decrypt usages
	var wrapOnlyUsages = Array.prototype.slice.call(wrapOnlyKey.usages);
	results.wrapOnlyHasEncrypt = wrapOnlyUsages.indexOf('encrypt') !== -1;
	results.wrapOnlyHasDecrypt = wrapOnlyUsages.indexOf('decrypt') !== -1;
	results.wrapOnlyHasWrapKey = wrapOnlyUsages.indexOf('wrapKey') !== -1;
	results.wrapOnlyHasUnwrapKey = wrapOnlyUsages.indexOf('unwrapKey') !== -1;

	var secretToWrap = await crypto.subtle.generateKey(
		{ name: 'AES-GCM', length: 128 },
		true,
		['encrypt', 'decrypt']
	);

	var wrapIv = new Uint8Array(12);
	crypto.getRandomValues(wrapIv);

	// Wrap with wrapKey-only key
	var wrappedWithWrapOnly = await crypto.subtle.wrapKey(
		'raw',
		secretToWrap,
		wrapOnlyKey,
		{ name: 'AES-GCM', iv: wrapIv }
	);
	results.wrapOnlyWrapped = wrappedWithWrapOnly instanceof ArrayBuffer && wrappedWithWrapOnly.byteLength > 0;

	// Unwrap with wrapKey-only key
	var unwrappedFromWrapOnly = await crypto.subtle.unwrapKey(
		'raw',
		wrappedWithWrapOnly,
		wrapOnlyKey,
		{ name: 'AES-GCM', iv: wrapIv },
		{ name: 'AES-GCM' },
		true,
		['encrypt', 'decrypt']
	);
	results.wrapOnlyUnwrappedAlgo = unwrappedFromWrapOnly.algorithm.name;

	// Verify the unwrapped key works: encrypt with original, decrypt with unwrapped
	var wrapTestIv = new Uint8Array(12);
	var wrapTestPlain = textEncode('wrapKey usage test');
	var wrapTestEnc = await crypto.subtle.encrypt(
		{ name: 'AES-GCM', iv: wrapTestIv },
		secretToWrap,
		wrapTestPlain
	);
	var wrapTestDec = await crypto.subtle.decrypt(
		{ name: 'AES-GCM', iv: wrapTestIv },
		unwrappedFromWrapOnly,
		wrapTestEnc
	);
	results.wrapOnlyRoundTrip = textDecode(wrapTestDec) === 'wrapKey usage test';

	__output(results);
}

run();
