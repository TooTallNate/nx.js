import { test } from '../src/tap';

test('TextEncoder encode', (t) => {
	const encoder = new TextEncoder();
	const result = encoder.encode('hello');
	t.equal(result.length, 5, 'length');
	t.equal(result[0], 0x68, 'h');
	t.equal(result[1], 0x65, 'e');
	t.equal(result[2], 0x6c, 'l');
	t.equal(result[3], 0x6c, 'l');
	t.equal(result[4], 0x6f, 'o');
});

test('TextEncoder encode empty string', (t) => {
	const encoder = new TextEncoder();
	const result = encoder.encode('');
	t.equal(result.length, 0, 'empty');
});

test('TextEncoder encode unicode', (t) => {
	const encoder = new TextEncoder();
	const result = encoder.encode('€');
	t.equal(result.length, 3, 'euro sign is 3 bytes in UTF-8');
	t.equal(result[0], 0xe2, 'byte 0');
	t.equal(result[1], 0x82, 'byte 1');
	t.equal(result[2], 0xac, 'byte 2');
});

test('TextDecoder decode', (t) => {
	const decoder = new TextDecoder();
	const bytes = new Uint8Array([0x68, 0x65, 0x6c, 0x6c, 0x6f]);
	t.equal(decoder.decode(bytes), 'hello', 'decoded');
});

test('TextDecoder decode empty', (t) => {
	const decoder = new TextDecoder();
	t.equal(decoder.decode(new Uint8Array(0)), '', 'empty');
	t.equal(decoder.decode(), '', 'no argument');
});

test('TextEncoder/TextDecoder roundtrip', (t) => {
	const encoder = new TextEncoder();
	const decoder = new TextDecoder();
	const strings = [
		'Hello, World!',
		'こんにちは',
		'🎮🎯🎲',
		'mixed ASCII and ünïcödë',
		'line1\nline2\ttab',
	];
	for (const str of strings) {
		t.equal(decoder.decode(encoder.encode(str)), str, `roundtrip: ${str}`);
	}
});

test('TextEncoder encoding property', (t) => {
	const encoder = new TextEncoder();
	t.equal(encoder.encoding, 'utf-8', 'encoding is utf-8');
});

test('TextDecoder encoding property', (t) => {
	const decoder = new TextDecoder();
	t.equal(decoder.encoding, 'utf-8', 'default encoding is utf-8');
});
