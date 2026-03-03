import { test } from '../src/tap';

test('wrapKey/unwrapKey AES with AES-GCM', async (t) => {
	const wrappingKey = await crypto.subtle.generateKey(
		{ name: 'AES-GCM', length: 256 },
		true,
		['wrapKey', 'unwrapKey'],
	);

	const keyToWrap = await crypto.subtle.generateKey(
		{ name: 'AES-GCM', length: 128 },
		true,
		['encrypt', 'decrypt'],
	);

	const iv = new Uint8Array(12);
	crypto.getRandomValues(iv);

	const wrapped = await crypto.subtle.wrapKey('raw', keyToWrap, wrappingKey, {
		name: 'AES-GCM',
		iv,
	});

	t.ok(wrapped instanceof ArrayBuffer, 'wrapped is ArrayBuffer');
	t.ok(wrapped.byteLength > 0, 'wrapped has data');

	const unwrapped = await crypto.subtle.unwrapKey(
		'raw',
		wrapped,
		wrappingKey,
		{ name: 'AES-GCM', iv },
		{ name: 'AES-GCM' },
		true,
		['encrypt', 'decrypt'],
	);

	t.equal(unwrapped.type, 'secret', 'unwrapped key type');
	t.equal(unwrapped.algorithm.name, 'AES-GCM', 'unwrapped key algo');
	t.equal(unwrapped.extractable, true, 'unwrapped key extractable');

	// Verify unwrapped key works: encrypt with original, decrypt with unwrapped
	const testIv = new Uint8Array(12);
	const plaintext = new TextEncoder().encode('wrap/unwrap test');
	const enc = await crypto.subtle.encrypt(
		{ name: 'AES-GCM', iv: testIv },
		keyToWrap,
		plaintext,
	);
	const dec = await crypto.subtle.decrypt(
		{ name: 'AES-GCM', iv: testIv },
		unwrapped,
		enc,
	);
	t.equal(
		new TextDecoder().decode(dec),
		'wrap/unwrap test',
		'unwrapped key decrypts data from original key',
	);
});

test('wrapKey/unwrapKey with wrapKey-only usages', async (t) => {
	const wrapOnlyKey = await crypto.subtle.generateKey(
		{ name: 'AES-GCM', length: 256 },
		true,
		['wrapKey', 'unwrapKey'],
	);

	const usages = Array.from(wrapOnlyKey.usages);
	t.notOk(usages.includes('encrypt'), 'no encrypt usage');
	t.notOk(usages.includes('decrypt'), 'no decrypt usage');
	t.ok(usages.includes('wrapKey'), 'has wrapKey usage');
	t.ok(usages.includes('unwrapKey'), 'has unwrapKey usage');

	const secretKey = await crypto.subtle.generateKey(
		{ name: 'AES-GCM', length: 128 },
		true,
		['encrypt', 'decrypt'],
	);

	const iv = new Uint8Array(12);
	crypto.getRandomValues(iv);

	const wrapped = await crypto.subtle.wrapKey('raw', secretKey, wrapOnlyKey, {
		name: 'AES-GCM',
		iv,
	});
	t.ok(
		wrapped instanceof ArrayBuffer && wrapped.byteLength > 0,
		'wrap succeeded with wrapKey-only key',
	);

	const unwrapped = await crypto.subtle.unwrapKey(
		'raw',
		wrapped,
		wrapOnlyKey,
		{ name: 'AES-GCM', iv },
		{ name: 'AES-GCM' },
		true,
		['encrypt', 'decrypt'],
	);
	t.equal(unwrapped.algorithm.name, 'AES-GCM', 'unwrapped key algo');

	// Verify round trip
	const testIv = new Uint8Array(12);
	const plaintext = new TextEncoder().encode('wrapKey usage test');
	const enc = await crypto.subtle.encrypt(
		{ name: 'AES-GCM', iv: testIv },
		secretKey,
		plaintext,
	);
	const dec = await crypto.subtle.decrypt(
		{ name: 'AES-GCM', iv: testIv },
		unwrapped,
		enc,
	);
	t.equal(
		new TextDecoder().decode(dec),
		'wrapKey usage test',
		'wrapKey-only round trip works',
	);
});
