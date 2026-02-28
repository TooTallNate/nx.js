import { suite } from 'uvu';
import * as assert from 'uvu/assert';

const test = suite('Uint8Array base64/hex');

// ========================================================================
// Helpers
// ========================================================================

function u8(...bytes: number[]): Uint8Array {
	return new Uint8Array(bytes);
}

function assertU8(actual: Uint8Array, expected: number[]) {
	assert.equal(Array.from(actual), expected);
}

function assertThrows(fn: () => void, errorType: string, msg?: string) {
	let threw = false;
	try {
		fn();
	} catch (e: any) {
		threw = true;
		assert.equal(e.constructor.name, errorType, msg);
	}
	assert.ok(threw, msg ?? `Expected ${errorType} to be thrown`);
}

// ========================================================================
// toHex()
// ========================================================================

test('toHex: basic encoding', () => {
	assert.equal(u8(202, 254, 208, 13).toHex(), 'cafed00d');
});

test('toHex: empty array', () => {
	assert.equal(u8().toHex(), '');
});

test('toHex: single byte', () => {
	assert.equal(u8(0).toHex(), '00');
	assert.equal(u8(255).toHex(), 'ff');
	assert.equal(u8(1).toHex(), '01');
	assert.equal(u8(16).toHex(), '10');
});

test('toHex: MDN example - RGB colors', () => {
	const data = u8(255, 0, 0, 0, 255, 0, 0, 0, 255);
	assert.equal(data.slice(0, 3).toHex(), 'ff0000');
	assert.equal(data.slice(3, 6).toHex(), '00ff00');
	assert.equal(data.slice(6, 9).toHex(), '0000ff');
});

// ========================================================================
// fromHex()
// ========================================================================

test('fromHex: basic decoding', () => {
	assertU8(Uint8Array.fromHex('cafed00d'), [202, 254, 208, 13]);
});

test('fromHex: empty string', () => {
	assertU8(Uint8Array.fromHex(''), []);
});

test('fromHex: uppercase', () => {
	assertU8(Uint8Array.fromHex('CAFEd00d'), [202, 254, 208, 13]);
});

test('fromHex: returns Uint8Array', () => {
	const result = Uint8Array.fromHex('cafed00d');
	assert.ok(result instanceof Uint8Array);
});

test('fromHex: throws SyntaxError on odd length', () => {
	assertThrows(() => Uint8Array.fromHex('abc'), 'SyntaxError');
});

test('fromHex: throws SyntaxError on invalid characters', () => {
	assertThrows(() => Uint8Array.fromHex('gg'), 'SyntaxError');
	assertThrows(() => Uint8Array.fromHex('0x'), 'SyntaxError');
});

test('fromHex: throws TypeError on non-string', () => {
	assertThrows(() => (Uint8Array as any).fromHex(123), 'TypeError');
});

// ========================================================================
// setFromHex()
// ========================================================================

test('setFromHex: basic', () => {
	const arr = new Uint8Array(8);
	const result = arr.setFromHex('cafed00d');
	assert.equal(result.read, 8);
	assert.equal(result.written, 4);
	assertU8(arr, [202, 254, 208, 13, 0, 0, 0, 0]);
});

test('setFromHex: array too small (partial fill)', () => {
	const arr = new Uint8Array(4);
	const result = arr.setFromHex('cafed00d-some random stuff');
	// Only 4 bytes fit, consuming 8 hex chars
	assert.equal(result.read, 8);
	assert.equal(result.written, 4);
	assertU8(arr, [202, 254, 208, 13]);
});

test('setFromHex: writing at offset via subarray', () => {
	const arr = new Uint8Array(8);
	const result = arr.subarray(2).setFromHex('cafed00d');
	assert.equal(result.read, 8);
	assert.equal(result.written, 4);
	assertU8(arr, [0, 0, 202, 254, 208, 13, 0, 0]);
});

test('setFromHex: throws SyntaxError on odd length', () => {
	const arr = new Uint8Array(8);
	assertThrows(() => arr.setFromHex('abc'), 'SyntaxError');
});

test('setFromHex: throws SyntaxError on invalid hex', () => {
	const arr = new Uint8Array(8);
	assertThrows(() => arr.setFromHex('zzzz'), 'SyntaxError');
});

test('setFromHex: empty string', () => {
	const arr = new Uint8Array(4);
	const result = arr.setFromHex('');
	assert.equal(result.read, 0);
	assert.equal(result.written, 0);
});

// ========================================================================
// toBase64()
// ========================================================================

test('toBase64: basic encoding', () => {
	assert.equal(u8(29, 233, 101, 161).toBase64(), 'HelloQ==');
});

test('toBase64: empty array', () => {
	assert.equal(u8().toBase64(), '');
});

test('toBase64: MDN example - omitPadding', () => {
	assert.equal(u8(29, 233, 101, 161).toBase64({ omitPadding: true }), 'HelloQ');
});

test('toBase64: MDN example - base64url alphabet', () => {
	const arr = u8(46, 139, 222, 255, 42, 46);
	assert.equal(arr.toBase64({ alphabet: 'base64url' }), 'Love_you');
});

test('toBase64: standard alphabet with /', () => {
	const arr = u8(46, 139, 222, 255, 42, 46);
	assert.equal(arr.toBase64(), 'Love/you');
});

test('toBase64: no padding needed (multiple of 3)', () => {
	assert.equal(u8(72, 101, 108).toBase64(), 'SGVs');
});

test('toBase64: one padding char (length % 3 == 2)', () => {
	assert.equal(u8(72, 101, 108, 108).toBase64(), 'SGVsbA==');
});

test('toBase64: omitPadding with base64url', () => {
	const arr = u8(46, 139, 222, 255, 42, 46);
	assert.equal(
		arr.toBase64({ alphabet: 'base64url', omitPadding: true }),
		'Love_you',
	);
});

test('toBase64: throws TypeError on invalid alphabet', () => {
	assertThrows(
		() => u8(1).toBase64({ alphabet: 'invalid' as any }),
		'TypeError',
	);
});

// ========================================================================
// fromBase64()
// ========================================================================

test('fromBase64: basic decoding', () => {
	assertU8(Uint8Array.fromBase64('SGVsbG8='), [72, 101, 108, 108, 111]);
});

test('fromBase64: empty string', () => {
	assertU8(Uint8Array.fromBase64(''), []);
});

test('fromBase64: returns Uint8Array', () => {
	const result = Uint8Array.fromBase64('SGVsbG8=');
	assert.ok(result instanceof Uint8Array);
});

test('fromBase64: whitespace is ignored', () => {
	assertU8(
		Uint8Array.fromBase64('PGI+ TURO PC9i Pg=='),
		[60, 98, 62, 77, 68, 78, 60, 47, 98, 62],
	);
});

test('fromBase64: MDN example - default loose handles incomplete chunks', () => {
	// 14 base64 chars, not a multiple of 4
	assertU8(
		Uint8Array.fromBase64('PGI+ TURO PC9i Ph'),
		[60, 98, 62, 77, 68, 78, 60, 47, 98, 62],
	);
});

test('fromBase64: base64url alphabet', () => {
	assertU8(
		Uint8Array.fromBase64('PGI-TUROPC9iPg', { alphabet: 'base64url' }),
		[60, 98, 62, 77, 68, 78, 60, 47, 98, 62],
	);
});

test('fromBase64: base64url rejects + and /', () => {
	assertThrows(
		() => Uint8Array.fromBase64('PGI+', { alphabet: 'base64url' }),
		'SyntaxError',
	);
	assertThrows(
		() => Uint8Array.fromBase64('PGI/', { alphabet: 'base64url' }),
		'SyntaxError',
	);
});

test('fromBase64: throws TypeError on non-string', () => {
	assertThrows(() => (Uint8Array as any).fromBase64(123), 'TypeError');
});

test('fromBase64: throws TypeError on invalid alphabet', () => {
	assertThrows(
		() => Uint8Array.fromBase64('AA==', { alphabet: 'invalid' as any }),
		'TypeError',
	);
});

test('fromBase64: throws TypeError on invalid lastChunkHandling', () => {
	assertThrows(
		() =>
			Uint8Array.fromBase64('AA==', {
				lastChunkHandling: 'invalid' as any,
			}),
		'TypeError',
	);
});

// ---- lastChunkHandling: "strict" ----

test('fromBase64: strict - padded input works', () => {
	assertU8(
		Uint8Array.fromBase64('PGI+ TURO PC9i Pg==', {
			lastChunkHandling: 'strict',
		}),
		[60, 98, 62, 77, 68, 78, 60, 47, 98, 62],
	);
});

test('fromBase64: strict - non-zero overflow bits throws SyntaxError', () => {
	// 'Ph' -> h = 0b100001, last 4 bits are 0001 (non-zero overflow)
	assertThrows(
		() =>
			Uint8Array.fromBase64('PGI+ TURO PC9i Ph==', {
				lastChunkHandling: 'strict',
			}),
		'SyntaxError',
	);
});

test('fromBase64: strict - missing padding throws SyntaxError', () => {
	assertThrows(
		() =>
			Uint8Array.fromBase64('PGI+ TURO PC9i Pg', {
				lastChunkHandling: 'strict',
			}),
		'SyntaxError',
	);
});

// ---- lastChunkHandling: "stop-before-partial" ----

test('fromBase64: stop-before-partial - complete chunk works', () => {
	assertU8(
		Uint8Array.fromBase64('PGI+ TURO PC9i', {
			lastChunkHandling: 'stop-before-partial',
		}),
		[60, 98, 62, 77, 68, 78, 60, 47, 98],
	);
});

test('fromBase64: stop-before-partial - padded chunk works', () => {
	assertU8(
		Uint8Array.fromBase64('PGI+ TURO PC9i Pg==', {
			lastChunkHandling: 'stop-before-partial',
		}),
		[60, 98, 62, 77, 68, 78, 60, 47, 98, 62],
	);
});

test('fromBase64: stop-before-partial - ignores unpadded partial chunk', () => {
	assertU8(
		Uint8Array.fromBase64('PGI+ TURO PC9i Pg', {
			lastChunkHandling: 'stop-before-partial',
		}),
		[60, 98, 62, 77, 68, 78, 60, 47, 98],
	);
});

test('fromBase64: stop-before-partial - ignores partial chunk with partial padding', () => {
	assertU8(
		Uint8Array.fromBase64('PGI+ TURO PC9i Pg=', {
			lastChunkHandling: 'stop-before-partial',
		}),
		[60, 98, 62, 77, 68, 78, 60, 47, 98],
	);
});

test('fromBase64: loose - single char chunk throws SyntaxError', () => {
	assertThrows(() => Uint8Array.fromBase64('P'), 'SyntaxError');
});

test('fromBase64: single char with = padding throws SyntaxError', () => {
	assertThrows(
		() =>
			Uint8Array.fromBase64('PGI+ TURO PC9i P=', {
				lastChunkHandling: 'stop-before-partial',
			}),
		'SyntaxError',
	);
});

test('fromBase64: invalid character throws SyntaxError', () => {
	assertThrows(() => Uint8Array.fromBase64('@@@@'), 'SyntaxError');
});

test('fromBase64: padding in wrong place throws SyntaxError', () => {
	assertThrows(() => Uint8Array.fromBase64('=AAA'), 'SyntaxError');
});

// ========================================================================
// setFromBase64()
// ========================================================================

test('setFromBase64: basic', () => {
	const arr = new Uint8Array(16);
	const result = arr.setFromBase64('PGI+ TURO PC9i Pg==');
	assert.equal(result.read, 19);
	assert.equal(result.written, 10);
	assertU8(arr, [60, 98, 62, 77, 68, 78, 60, 47, 98, 62, 0, 0, 0, 0, 0, 0]);
});

test('setFromBase64: array too small', () => {
	const arr = new Uint8Array(8);
	const result = arr.setFromBase64('PGI+ TURO PC9i Pg==');
	assert.equal(result.read, 9);
	assert.equal(result.written, 6);
	assertU8(arr, [60, 98, 62, 77, 68, 78, 0, 0]);
});

test('setFromBase64: writing at offset via subarray', () => {
	const arr = new Uint8Array(16);
	const result = arr.subarray(2).setFromBase64('PGI+ TURO PC9i Pg==');
	assert.equal(result.read, 19);
	assert.equal(result.written, 10);
	assertU8(arr, [0, 0, 60, 98, 62, 77, 68, 78, 60, 47, 98, 62, 0, 0, 0, 0]);
});

test('setFromBase64: empty string', () => {
	const arr = new Uint8Array(4);
	const result = arr.setFromBase64('');
	assert.equal(result.read, 0);
	assert.equal(result.written, 0);
});

test('setFromBase64: stop-before-partial for streaming', () => {
	const arr = new Uint8Array(16);
	// "SG Vsb" = "SGVsb" (5 base64 chars), only 1 full chunk (4 chars) decoded
	const result = arr.setFromBase64('SGVsb', {
		lastChunkHandling: 'stop-before-partial',
	});
	assert.equal(result.read, 4);
	assert.equal(result.written, 3);
	assertU8(arr.subarray(0, 3), [72, 101, 108]);
});

test('setFromBase64: strict with padding', () => {
	const arr = new Uint8Array(16);
	const result = arr.setFromBase64('SGVsbA==', {
		lastChunkHandling: 'strict',
	});
	assert.equal(result.written, 4);
	assertU8(arr.subarray(0, 4), [72, 101, 108, 108]);
});

test('setFromBase64: strict without padding throws', () => {
	const arr = new Uint8Array(16);
	assertThrows(
		() => arr.setFromBase64('SGVsbA', { lastChunkHandling: 'strict' }),
		'SyntaxError',
	);
});

// ========================================================================
// Round-trip tests
// ========================================================================

test('round-trip: toBase64 -> fromBase64', () => {
	const original = u8(0, 1, 2, 127, 128, 255);
	const b64 = original.toBase64();
	assertU8(Uint8Array.fromBase64(b64), Array.from(original));
});

test('round-trip: toBase64 base64url -> fromBase64 base64url', () => {
	const original = u8(46, 139, 222, 255, 42, 46);
	const b64 = original.toBase64({ alphabet: 'base64url' });
	assertU8(
		Uint8Array.fromBase64(b64, { alphabet: 'base64url' }),
		Array.from(original),
	);
});

test('round-trip: toBase64 omitPadding -> fromBase64', () => {
	const original = u8(72, 101, 108, 108);
	const b64 = original.toBase64({ omitPadding: true });
	assertU8(Uint8Array.fromBase64(b64), Array.from(original));
});

test('round-trip: toHex -> fromHex', () => {
	const original = u8(0, 1, 127, 128, 255);
	const hex = original.toHex();
	assertU8(Uint8Array.fromHex(hex), Array.from(original));
});

// ========================================================================
// Edge cases
// ========================================================================

test('toBase64: all zeroes', () => {
	assert.equal(u8(0, 0, 0).toBase64(), 'AAAA');
});

test('toBase64: all 0xff', () => {
	assert.equal(u8(255, 255, 255).toBase64(), '////');
});

test('toBase64: all 0xff base64url', () => {
	assert.equal(u8(255, 255, 255).toBase64({ alphabet: 'base64url' }), '____');
});

test('fromBase64: whitespace types - tab, newline, CR, FF, space', () => {
	// All 5 ASCII whitespace chars that should be stripped
	assertU8(
		Uint8Array.fromBase64('S\tG\nV\rs\x0cb\x20A=='),
		[72, 101, 108, 108],
	);
});

test('fromHex: no whitespace stripping', () => {
	// Hex does NOT strip whitespace per spec
	assertThrows(() => Uint8Array.fromHex('ca fe'), 'SyntaxError');
});

test('setFromHex: excess valid hex beyond array capacity', () => {
	const arr = new Uint8Array(2);
	const result = arr.setFromHex('cafed00d');
	assert.equal(result.read, 4);
	assert.equal(result.written, 2);
	assertU8(arr, [202, 254]);
});

test('setFromBase64: exact fit', () => {
	const arr = new Uint8Array(3);
	const result = arr.setFromBase64('AAAA');
	assert.equal(result.read, 4);
	assert.equal(result.written, 3);
	assertU8(arr, [0, 0, 0]);
});

test('setFromBase64: zero-length array', () => {
	const arr = new Uint8Array(0);
	const result = arr.setFromBase64('AAAA');
	assert.equal(result.read, 0);
	assert.equal(result.written, 0);
});

test('setFromHex: zero-length array', () => {
	const arr = new Uint8Array(0);
	const result = arr.setFromHex('cafe');
	assert.equal(result.read, 0);
	assert.equal(result.written, 0);
});

test.run();
