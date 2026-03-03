import { test } from '../src/tap';

function toHex(buf: ArrayBuffer): string {
	const bytes = new Uint8Array(buf);
	let hex = '';
	for (let i = 0; i < bytes.length; i++) {
		hex += bytes[i].toString(16).padStart(2, '0');
	}
	return hex;
}

test('crypto.randomUUID format', (t) => {
	const uuid = crypto.randomUUID();
	t.equal(uuid.length, 36, 'length is 36');
	t.match(
		uuid,
		/^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/,
		'UUID v4 format',
	);
});

test('crypto.randomUUID uniqueness', (t) => {
	const a = crypto.randomUUID();
	const b = crypto.randomUUID();
	t.notEqual(a, b, 'two UUIDs should differ');
});

test('crypto.getRandomValues', (t) => {
	const buf = new Uint8Array(16);
	const result = crypto.getRandomValues(buf);
	t.equal(result, buf, 'returns same buffer');
	// Extremely unlikely all 16 bytes are zero
	let allZero = true;
	for (let i = 0; i < buf.length; i++) {
		if (buf[i] !== 0) {
			allZero = false;
			break;
		}
	}
	t.notOk(allZero, 'buffer is not all zeros');
});

test('crypto.getRandomValues with no arguments throws', (t) => {
	let threw = false;
	try {
		// @ts-expect-error intentionally passing no arguments
		crypto.getRandomValues();
	} catch {
		threw = true;
	}
	t.ok(threw, 'throws when called with no arguments');
});

test('crypto.subtle.digest SHA-256', async (t) => {
	const data = new TextEncoder().encode('abc');
	const hash = await crypto.subtle.digest('SHA-256', data);
	t.equal(hash.byteLength, 32, 'SHA-256 produces 32 bytes');
	t.equal(
		toHex(hash),
		'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad',
		'SHA-256 of "abc"',
	);
});

test('crypto.subtle.digest SHA-1', async (t) => {
	const data = new TextEncoder().encode('abc');
	const hash = await crypto.subtle.digest('SHA-1', data);
	t.equal(hash.byteLength, 20, 'SHA-1 produces 20 bytes');
	t.equal(
		toHex(hash),
		'a9993e364706816aba3e25717850c26c9cd0d89d',
		'SHA-1 of "abc"',
	);
});

test('crypto.subtle.digest SHA-384', async (t) => {
	const data = new TextEncoder().encode('abc');
	const hash = await crypto.subtle.digest('SHA-384', data);
	t.equal(hash.byteLength, 48, 'SHA-384 produces 48 bytes');
	t.equal(
		toHex(hash),
		'cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7',
		'SHA-384 of "abc"',
	);
});

test('crypto.subtle.digest SHA-512', async (t) => {
	const data = new TextEncoder().encode('abc');
	const hash = await crypto.subtle.digest('SHA-512', data);
	t.equal(hash.byteLength, 64, 'SHA-512 produces 64 bytes');
	t.equal(
		toHex(hash),
		'ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f',
		'SHA-512 of "abc"',
	);
});

test('crypto.subtle.digest empty input', async (t) => {
	const hash = await crypto.subtle.digest('SHA-256', new Uint8Array(0));
	t.equal(
		toHex(hash),
		'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855',
		'SHA-256 of empty input',
	);
});
