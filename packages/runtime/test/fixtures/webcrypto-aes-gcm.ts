import { test } from '../src/tap';

test('AES-GCM-128 encrypt/decrypt round trip', async (t) => {
	const keyData = new Uint8Array(16);
	const iv = new Uint8Array(12);
	const plaintext = new TextEncoder().encode('Hello AES-GCM!');

	const key = await crypto.subtle.importKey(
		'raw',
		keyData,
		{ name: 'AES-GCM' },
		true,
		['encrypt', 'decrypt'],
	);

	const encrypted = await crypto.subtle.encrypt(
		{ name: 'AES-GCM', iv },
		key,
		plaintext,
	);

	t.equal(
		encrypted.byteLength,
		plaintext.byteLength + 16,
		'ciphertext = plaintext + 16 byte tag',
	);

	const decrypted = await crypto.subtle.decrypt(
		{ name: 'AES-GCM', iv },
		key,
		encrypted,
	);
	t.equal(
		new TextDecoder().decode(decrypted),
		'Hello AES-GCM!',
		'round trip matches',
	);
});

test('AES-GCM-256 encrypt/decrypt round trip', async (t) => {
	const keyData = new Uint8Array(32);
	const iv = new Uint8Array(12);
	const plaintext = new TextEncoder().encode('Hello AES-GCM!');

	const key = await crypto.subtle.importKey(
		'raw',
		keyData,
		{ name: 'AES-GCM' },
		true,
		['encrypt', 'decrypt'],
	);

	const encrypted = await crypto.subtle.encrypt(
		{ name: 'AES-GCM', iv },
		key,
		plaintext,
	);

	const decrypted = await crypto.subtle.decrypt(
		{ name: 'AES-GCM', iv },
		key,
		encrypted,
	);
	t.equal(
		new TextDecoder().decode(decrypted),
		'Hello AES-GCM!',
		'256-bit round trip',
	);
});

test('AES-GCM with additionalData (AAD)', async (t) => {
	const key = await crypto.subtle.importKey(
		'raw',
		new Uint8Array(16),
		{ name: 'AES-GCM' },
		false,
		['encrypt', 'decrypt'],
	);
	const iv = new Uint8Array(12);
	const aad = new TextEncoder().encode('authenticated header');
	const plaintext = new TextEncoder().encode('Hello AES-GCM!');

	const encrypted = await crypto.subtle.encrypt(
		{ name: 'AES-GCM', iv, additionalData: aad },
		key,
		plaintext,
	);

	const decrypted = await crypto.subtle.decrypt(
		{ name: 'AES-GCM', iv, additionalData: aad },
		key,
		encrypted,
	);
	t.equal(new TextDecoder().decode(decrypted), 'Hello AES-GCM!', 'AAD round trip');

	// Wrong AAD should fail
	const wrongAAD = new TextEncoder().encode('wrong header');
	let failed = false;
	try {
		await crypto.subtle.decrypt(
			{ name: 'AES-GCM', iv, additionalData: wrongAAD },
			key,
			encrypted,
		);
	} catch {
		failed = true;
	}
	t.ok(failed, 'wrong AAD causes decrypt failure');
});

test('AES-GCM wrong key fails', async (t) => {
	const key = await crypto.subtle.importKey(
		'raw',
		new Uint8Array(16),
		{ name: 'AES-GCM' },
		false,
		['encrypt', 'decrypt'],
	);
	const iv = new Uint8Array(12);
	const encrypted = await crypto.subtle.encrypt(
		{ name: 'AES-GCM', iv },
		key,
		new TextEncoder().encode('test'),
	);

	const wrongKeyData = new Uint8Array(16);
	wrongKeyData[0] = 0xff;
	const wrongKey = await crypto.subtle.importKey(
		'raw',
		wrongKeyData,
		{ name: 'AES-GCM' },
		false,
		['decrypt'],
	);

	let failed = false;
	try {
		await crypto.subtle.decrypt({ name: 'AES-GCM', iv }, wrongKey, encrypted);
	} catch {
		failed = true;
	}
	t.ok(failed, 'wrong key causes decrypt failure');
});

test('AES-GCM custom tagLength (96 bits)', async (t) => {
	const key = await crypto.subtle.importKey(
		'raw',
		new Uint8Array(16),
		{ name: 'AES-GCM' },
		false,
		['encrypt', 'decrypt'],
	);
	const iv = new Uint8Array(12);
	const plaintext = new TextEncoder().encode('Hello AES-GCM!');

	const encrypted = await crypto.subtle.encrypt(
		{ name: 'AES-GCM', iv, tagLength: 96 },
		key,
		plaintext,
	);
	t.equal(
		encrypted.byteLength,
		plaintext.byteLength + 12,
		'96-bit tag = 12 bytes',
	);

	const decrypted = await crypto.subtle.decrypt(
		{ name: 'AES-GCM', iv, tagLength: 96 },
		key,
		encrypted,
	);
	t.equal(new TextDecoder().decode(decrypted), 'Hello AES-GCM!', 'round trip with 96-bit tag');
});

test('AES-GCM key properties', async (t) => {
	const key = await crypto.subtle.importKey(
		'raw',
		new Uint8Array(16),
		{ name: 'AES-GCM' },
		true,
		['encrypt', 'decrypt'],
	);

	t.equal(key.type, 'secret', 'key type');
	t.equal(key.algorithm.name, 'AES-GCM', 'algorithm name');
	t.equal((key.algorithm as AesKeyAlgorithm).length, 128, 'key length');
	t.equal(key.extractable, true, 'extractable');
	t.deepEqual(Array.from(key.usages).sort(), ['decrypt', 'encrypt'], 'usages');
});

test('AES-GCM generateKey round trip', async (t) => {
	const key = await crypto.subtle.generateKey(
		{ name: 'AES-GCM', length: 256 },
		true,
		['encrypt', 'decrypt'],
	);
	const iv = new Uint8Array(12);
	const plaintext = new TextEncoder().encode('Hello AES-GCM!');

	const encrypted = await crypto.subtle.encrypt(
		{ name: 'AES-GCM', iv },
		key,
		plaintext,
	);
	const decrypted = await crypto.subtle.decrypt(
		{ name: 'AES-GCM', iv },
		key,
		encrypted,
	);
	t.equal(new TextDecoder().decode(decrypted), 'Hello AES-GCM!', 'generateKey round trip');
	t.equal(key.algorithm.name, 'AES-GCM', 'generated key algorithm');
});

test('AES-GCM exportKey round trip', async (t) => {
	const keyData = new Uint8Array(16);
	const iv = new Uint8Array(12);
	const plaintext = new TextEncoder().encode('Hello AES-GCM!');

	const key = await crypto.subtle.importKey(
		'raw',
		keyData,
		{ name: 'AES-GCM' },
		true,
		['encrypt', 'decrypt'],
	);

	const enc1 = await crypto.subtle.encrypt({ name: 'AES-GCM', iv }, key, plaintext);

	const exported = await crypto.subtle.exportKey('raw', key);
	const reimported = await crypto.subtle.importKey(
		'raw',
		exported,
		{ name: 'AES-GCM' },
		true,
		['encrypt', 'decrypt'],
	);

	const enc2 = await crypto.subtle.encrypt({ name: 'AES-GCM', iv }, reimported, plaintext);
	t.equal(new Uint8Array(enc1).toHex(), new Uint8Array(enc2).toHex(), 'exported/reimported key produces same ciphertext');
});
