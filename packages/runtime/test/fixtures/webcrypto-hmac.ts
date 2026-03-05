import { test } from '../src/tap';

test('HMAC sign/verify with SHA-256', async (t) => {
	const keyData = new Uint8Array(32); // all zeros
	const data = new TextEncoder().encode('hello');

	const key = await crypto.subtle.importKey(
		'raw',
		keyData,
		{ name: 'HMAC', hash: { name: 'SHA-256' } },
		false,
		['sign', 'verify'],
	);

	const sig = await crypto.subtle.sign('HMAC', key, data);
	t.equal(sig.byteLength, 32, 'SHA-256 HMAC is 32 bytes');

	const valid = await crypto.subtle.verify('HMAC', key, sig, data);
	t.ok(valid, 'signature verifies');

	const tampered = await crypto.subtle.verify(
		'HMAC',
		key,
		sig,
		new TextEncoder().encode('world'),
	);
	t.notOk(tampered, 'tampered data fails');
});

test('HMAC sign/verify with SHA-1', async (t) => {
	const keyData = new Uint8Array(32);
	const data = new TextEncoder().encode('hello');

	const key = await crypto.subtle.importKey(
		'raw',
		keyData,
		{ name: 'HMAC', hash: { name: 'SHA-1' } },
		false,
		['sign', 'verify'],
	);

	const sig = await crypto.subtle.sign('HMAC', key, data);
	t.equal(sig.byteLength, 20, 'SHA-1 HMAC is 20 bytes');

	const valid = await crypto.subtle.verify('HMAC', key, sig, data);
	t.ok(valid, 'SHA-1 signature verifies');
});

test('HMAC sign/verify with SHA-384', async (t) => {
	const keyData = new Uint8Array(32);
	const data = new TextEncoder().encode('hello');

	const key = await crypto.subtle.importKey(
		'raw',
		keyData,
		{ name: 'HMAC', hash: { name: 'SHA-384' } },
		false,
		['sign', 'verify'],
	);

	const sig = await crypto.subtle.sign('HMAC', key, data);
	t.equal(sig.byteLength, 48, 'SHA-384 HMAC is 48 bytes');

	const valid = await crypto.subtle.verify('HMAC', key, sig, data);
	t.ok(valid, 'SHA-384 signature verifies');
});

test('HMAC sign/verify with SHA-512', async (t) => {
	const keyData = new Uint8Array(32);
	const data = new TextEncoder().encode('hello');

	const key = await crypto.subtle.importKey(
		'raw',
		keyData,
		{ name: 'HMAC', hash: { name: 'SHA-512' } },
		false,
		['sign', 'verify'],
	);

	const sig = await crypto.subtle.sign('HMAC', key, data);
	t.equal(sig.byteLength, 64, 'SHA-512 HMAC is 64 bytes');

	const valid = await crypto.subtle.verify('HMAC', key, sig, data);
	t.ok(valid, 'SHA-512 signature verifies');
});

test('HMAC key properties', async (t) => {
	const key = await crypto.subtle.importKey(
		'raw',
		new Uint8Array(32),
		{ name: 'HMAC', hash: { name: 'SHA-256' } },
		true,
		['sign', 'verify'],
	);

	t.equal(key.type, 'secret', 'key type');
	t.equal(key.extractable, true, 'extractable');
	t.equal(key.algorithm.name, 'HMAC', 'algorithm name');
	t.deepEqual(Array.from(key.usages).sort(), ['sign', 'verify'], 'usages');
});

test('HMAC exportKey round trip', async (t) => {
	const keyData = new Uint8Array(32);
	const key = await crypto.subtle.importKey(
		'raw',
		keyData,
		{ name: 'HMAC', hash: { name: 'SHA-256' } },
		true,
		['sign', 'verify'],
	);

	const exported = await crypto.subtle.exportKey('raw', key);
	t.equal(exported.byteLength, 32, 'exported key is 32 bytes');
	t.equal(
		new Uint8Array(exported).toHex(),
		new Uint8Array(keyData.buffer).toHex(),
		'exported key matches original',
	);
});
