import { test } from '../src/tap';

// Conformance for atob() / btoa(). These run in BOTH nxjs-test and Chrome and
// are compared assertion-by-assertion, so any divergence from Chrome's
// (WHATWG forgiving-base64 + binary-string) behavior is caught.
//
// The core invariant the runtime previously got wrong: atob/btoa operate on
// "binary strings" where each char is a byte 0..255 (Latin-1), NOT UTF-8. So
// atob(btoa(s)) === s for any 0..255-byte string, including bytes >= 0x80, and
// btoa throws for chars > 0xFF.

// Build a string of raw bytes (each char = one byte value).
function bytesToBinaryString(bytes: number[]): string {
	let s = '';
	for (const b of bytes) s += String.fromCharCode(b);
	return s;
}
function binaryStringToBytes(s: string): number[] {
	const out: number[] = [];
	for (let i = 0; i < s.length; i++) out.push(s.charCodeAt(i));
	return out;
}

test('btoa - basic ASCII', (t) => {
	t.equal(btoa('hello'), 'aGVsbG8=', 'btoa("hello")');
	t.equal(btoa(''), '', 'btoa("")');
	t.equal(btoa('f'), 'Zg==', 'one byte -> two padding');
	t.equal(btoa('fo'), 'Zm8=', 'two bytes -> one padding');
	t.equal(btoa('foo'), 'Zm9v', 'three bytes -> no padding');
});

test('atob - basic ASCII', (t) => {
	t.equal(atob('aGVsbG8='), 'hello', 'atob -> "hello"');
	t.equal(atob(''), '', 'atob("")');
	t.equal(atob('Zg=='), 'f', 'one padding');
	t.equal(atob('Zm8='), 'fo', 'two padding... err one');
	t.equal(atob('Zm9v'), 'foo', 'no padding');
});

test('atob/btoa - round-trips all byte values 0..255', (t) => {
	// THE regression: every byte must survive btoa->atob, including >= 0x80.
	const allBytes: number[] = [];
	for (let i = 0; i < 256; i++) allBytes.push(i);
	const bin = bytesToBinaryString(allBytes);
	const encoded = btoa(bin);
	const decoded = atob(encoded);
	t.equal(decoded.length, 256, 'decoded length is 256');
	const got = binaryStringToBytes(decoded);
	let ok = true;
	let firstBad = -1;
	for (let i = 0; i < 256; i++) {
		if (got[i] !== i) {
			ok = false;
			firstBad = i;
			break;
		}
	}
	t.ok(ok, 'all 256 byte values round-trip (first mismatch: ' + firstBad + ')');
});

test('atob - decoded high bytes have the right char codes', (t) => {
	// btoa of bytes [0xFF, 0x80, 0x00, 0x7F] then atob back.
	const enc = btoa(bytesToBinaryString([0xff, 0x80, 0x00, 0x7f]));
	const dec = atob(enc);
	t.equal(dec.length, 4, 'length 4');
	t.equal(dec.charCodeAt(0), 0xff, 'byte 0 = 0xFF (was mangled by UTF-8)');
	t.equal(dec.charCodeAt(1), 0x80, 'byte 1 = 0x80');
	t.equal(dec.charCodeAt(2), 0x00, 'byte 2 = 0x00');
	t.equal(dec.charCodeAt(3), 0x7f, 'byte 3 = 0x7F');
});

test('btoa - encodes each char as a single byte 0..255', (t) => {
	// A Latin-1 string \x00\xFF must encode as the two bytes [0x00, 0xFF],
	// i.e. "AP8=" — NOT as UTF-8 of U+00FF (which would give 3+ bytes).
	t.equal(btoa('\x00\xff'), 'AP8=', 'btoa("\\x00\\xff") == "AP8="');
	t.equal(btoa('\xff'), '/w==', 'btoa("\\xff") == "/w=="');
	t.equal(btoa('\x80'), 'gA==', 'btoa("\\x80") == "gA=="');
});

test('btoa - char > 0xFF throws InvalidCharacterError', (t) => {
	// '\u0100' (256) is out of Latin-1 range -> spec requires throwing.
	t.throws(() => btoa('\u0100'), undefined, 'btoa("\\u0100") throws');
	t.throws(() => btoa('abc\u20ac'), undefined, 'euro sign throws');
	t.throws(() => btoa('\uD83D\uDE00'), undefined, 'emoji (surrogates) throws');
});

test('btoa - throws with name InvalidCharacterError', (t) => {
	let name = '';
	try {
		btoa('\u0100');
	} catch (e) {
		name = (e as { name?: string }).name ?? '';
	}
	t.equal(name, 'InvalidCharacterError', 'error name is InvalidCharacterError');
});

test('atob - invalid base64 throws', (t) => {
	// A single stray char (length % 4 == 1) is not valid base64.
	t.throws(() => atob('a'), undefined, 'atob("a") throws (bad length)');
	// Non-base64 character.
	t.throws(() => atob('!!!!'), undefined, 'atob("!!!!") throws');
});

test('atob - throws with name InvalidCharacterError', (t) => {
	let name = '';
	try {
		atob('a');
	} catch (e) {
		name = (e as { name?: string }).name ?? '';
	}
	t.equal(name, 'InvalidCharacterError', 'error name is InvalidCharacterError');
});

test('btoa - argument is stringified', (t) => {
	// Non-string args are ToString'd first.
	t.equal(btoa(123 as unknown as string), btoa('123'), 'number -> "123"');
	t.equal(btoa(null as unknown as string), btoa('null'), 'null -> "null"');
	t.equal(
		btoa(true as unknown as string),
		btoa('true'),
		'boolean -> "true"',
	);
});

test('atob/btoa - longer binary round-trip', (t) => {
	// A pseudo-random byte sequence (deterministic), all 0..255.
	const bytes: number[] = [];
	let x = 12345;
	for (let i = 0; i < 1000; i++) {
		x = (x * 1103515245 + 12345) & 0x7fffffff;
		bytes.push(x & 0xff);
	}
	const bin = bytesToBinaryString(bytes);
	const rt = atob(btoa(bin));
	t.equal(rt, bin, '1000-byte binary string round-trips');
});
