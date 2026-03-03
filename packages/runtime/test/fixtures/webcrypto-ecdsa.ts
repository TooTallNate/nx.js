import { test } from '../src/tap';

test('ECDSA P-256 sign/verify', async (t) => {
	const pair = await crypto.subtle.generateKey(
		{ name: 'ECDSA', namedCurve: 'P-256' },
		true,
		['sign', 'verify'],
	);

	const data = new TextEncoder().encode('hello world');
	const sig = await crypto.subtle.sign(
		{ name: 'ECDSA', hash: { name: 'SHA-256' } },
		pair.privateKey,
		data,
	);

	t.equal(sig.byteLength, 64, 'P-256 signature is 64 bytes');

	const valid = await crypto.subtle.verify(
		{ name: 'ECDSA', hash: { name: 'SHA-256' } },
		pair.publicKey,
		sig,
		data,
	);
	t.ok(valid, 'signature verifies');

	const invalid = await crypto.subtle.verify(
		{ name: 'ECDSA', hash: { name: 'SHA-256' } },
		pair.publicKey,
		sig,
		new TextEncoder().encode('wrong data'),
	);
	t.notOk(invalid, 'wrong data fails verification');
});

test('ECDSA P-256 key properties', async (t) => {
	const pair = await crypto.subtle.generateKey(
		{ name: 'ECDSA', namedCurve: 'P-256' },
		true,
		['sign', 'verify'],
	);

	t.equal(pair.privateKey.type, 'private', 'private key type');
	t.equal(pair.publicKey.type, 'public', 'public key type');
	t.equal(pair.privateKey.algorithm.name, 'ECDSA', 'private algo name');
	t.equal(pair.publicKey.algorithm.name, 'ECDSA', 'public algo name');
	t.equal(pair.privateKey.extractable, true, 'private extractable');
	t.equal(pair.publicKey.extractable, true, 'public extractable');
});

test('ECDSA P-256 export/reimport public key verify', async (t) => {
	const pair = await crypto.subtle.generateKey(
		{ name: 'ECDSA', namedCurve: 'P-256' },
		true,
		['sign', 'verify'],
	);

	const data = new TextEncoder().encode('hello world');
	const sig = await crypto.subtle.sign(
		{ name: 'ECDSA', hash: { name: 'SHA-256' } },
		pair.privateKey,
		data,
	);

	const exported = await crypto.subtle.exportKey('raw', pair.publicKey);
	t.equal(exported.byteLength, 65, 'uncompressed public key is 65 bytes');
	t.equal(new Uint8Array(exported)[0], 0x04, 'starts with 0x04 prefix');

	const reimported = await crypto.subtle.importKey(
		'raw',
		exported,
		{ name: 'ECDSA', namedCurve: 'P-256' },
		true,
		['verify'],
	);

	const valid = await crypto.subtle.verify(
		{ name: 'ECDSA', hash: { name: 'SHA-256' } },
		reimported,
		sig,
		data,
	);
	t.ok(valid, 'reimported public key verifies signature');
});

test('ECDSA P-384 sign/verify', async (t) => {
	const pair = await crypto.subtle.generateKey(
		{ name: 'ECDSA', namedCurve: 'P-384' },
		true,
		['sign', 'verify'],
	);

	const data = new TextEncoder().encode('hello world');
	const sig = await crypto.subtle.sign(
		{ name: 'ECDSA', hash: { name: 'SHA-384' } },
		pair.privateKey,
		data,
	);

	t.equal(sig.byteLength, 96, 'P-384 signature is 96 bytes');

	const valid = await crypto.subtle.verify(
		{ name: 'ECDSA', hash: { name: 'SHA-384' } },
		pair.publicKey,
		sig,
		data,
	);
	t.ok(valid, 'P-384 signature verifies');
});
