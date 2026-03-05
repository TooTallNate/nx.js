import { test } from '../src/tap';

test('TextDecoder - encoding property', (t) => {
	const d = new TextDecoder();
	t.equal(d.encoding, 'utf-8', 'encoding');
	t.equal(d.fatal, false, 'fatal default');
	t.equal(d.ignoreBOM, false, 'ignoreBOM default');
});

test('TextDecoder - ASCII', (t) => {
	const d = new TextDecoder();
	t.equal(d.decode(new Uint8Array([72, 101, 108, 108, 111])), 'Hello', 'Hello');
});

test('TextDecoder - empty input', (t) => {
	const d = new TextDecoder();
	t.equal(d.decode(), '', 'no arg');
	t.equal(d.decode(new Uint8Array(0)), '', 'empty array');
});

test('TextDecoder - 2-byte UTF-8', (t) => {
	const d = new TextDecoder();
	// é = C3 A9
	t.equal(d.decode(new Uint8Array([0xc3, 0xa9])), 'é', 'é');
});

test('TextDecoder - 3-byte Euro sign', (t) => {
	const d = new TextDecoder();
	t.equal(d.decode(new Uint8Array([0xe2, 0x82, 0xac])), '€', 'Euro sign');
});

test('TextDecoder - 3-byte CJK', (t) => {
	const d = new TextDecoder();
	const encoded = new TextEncoder().encode('你好世界');
	t.equal(d.decode(encoded), '你好世界', 'CJK');
});

test('TextDecoder - 4-byte emoji', (t) => {
	const d = new TextDecoder();
	const encoded = new TextEncoder().encode('😀🎉');
	t.equal(d.decode(encoded), '😀🎉', 'emoji');
});

test('TextDecoder - replacement character for invalid bytes', (t) => {
	const d = new TextDecoder();
	const result = d.decode(new Uint8Array([0xff]));
	t.equal(result, '\uFFFD', 'invalid byte replaced');
});

test('TextDecoder - fatal mode throws on invalid', (t) => {
	const d = new TextDecoder('utf-8', { fatal: true });
	t.equal(d.fatal, true, 'fatal is true');
	t.throws(
		() => {
			d.decode(new Uint8Array([0xff]));
		},
		undefined,
		'throws on invalid byte',
	);
});

test('TextDecoder - fatal mode valid input', (t) => {
	const d = new TextDecoder('utf-8', { fatal: true });
	const encoded = new TextEncoder().encode('Hello 你好 😀');
	t.equal(
		d.decode(encoded),
		'Hello 你好 😀',
		'valid input works in fatal mode',
	);
});

test('TextDecoder - strips BOM by default', (t) => {
	const d = new TextDecoder();
	// BOM (EF BB BF) + "Hi"
	const bytes = new Uint8Array([0xef, 0xbb, 0xbf, 0x48, 0x69]);
	t.equal(d.decode(bytes), 'Hi', 'BOM stripped');
});

test('TextDecoder - ignoreBOM preserves BOM', (t) => {
	const d = new TextDecoder('utf-8', { ignoreBOM: true });
	t.equal(d.ignoreBOM, true, 'ignoreBOM is true');
	const bytes = new Uint8Array([0xef, 0xbb, 0xbf, 0x48, 0x69]);
	t.equal(d.decode(bytes), '\uFEFF' + 'Hi', 'BOM preserved');
});

test('TextDecoder - 3-byte invalid continuation backtrack', (t) => {
	const d = new TextDecoder();
	// 3-byte lead (0xe2), then invalid continuation (0x41 = 'A') which is a valid ASCII byte
	// Should produce U+FFFD for the broken sequence, then 'A' from backtracking
	const result = d.decode(new Uint8Array([0xe2, 0x41]));
	t.equal(result, '\uFFFDA', 'backtrack invalid byte2 in 3-byte sequence');
});

test('TextDecoder - 3-byte invalid third byte backtrack', (t) => {
	const d = new TextDecoder();
	// 3-byte lead (0xe2), valid continuation (0x82), then invalid byte3 (0x41 = 'A')
	// Should produce U+FFFD for the broken sequence, then 'A' from backtracking
	const result = d.decode(new Uint8Array([0xe2, 0x82, 0x41]));
	t.equal(result, '\uFFFDA', 'backtrack invalid byte3 in 3-byte sequence');
});

test('TextDecoder - 4-byte invalid continuation backtrack', (t) => {
	const d = new TextDecoder();
	// 4-byte lead (0xf0), then invalid continuation (0x41 = 'A')
	const result = d.decode(new Uint8Array([0xf0, 0x41]));
	t.equal(result, '\uFFFDA', 'backtrack invalid byte2 in 4-byte sequence');
});

test('TextDecoder - 4-byte invalid third byte backtrack', (t) => {
	const d = new TextDecoder();
	// 4-byte lead (0xf0), valid byte2 (0x9f), then invalid byte3 (0x41 = 'A')
	const result = d.decode(new Uint8Array([0xf0, 0x9f, 0x41]));
	t.equal(result, '\uFFFDA', 'backtrack invalid byte3 in 4-byte sequence');
});

test('TextDecoder - 4-byte invalid fourth byte backtrack', (t) => {
	const d = new TextDecoder();
	// 4-byte lead (0xf0), valid byte2 (0x9f), valid byte3 (0x98), then invalid byte4 (0x41 = 'A')
	const result = d.decode(new Uint8Array([0xf0, 0x9f, 0x98, 0x41]));
	t.equal(result, '\uFFFDA', 'backtrack invalid byte4 in 4-byte sequence');
});

test('TextDecoder - invalid continuation followed by multi-byte sequence', (t) => {
	const d = new TextDecoder();
	// 3-byte lead (0xe2), then a 2-byte lead (0xc3, 0xa9 = 'é')
	// Should produce U+FFFD for broken 3-byte, then 'é' from backtracking
	const result = d.decode(new Uint8Array([0xe2, 0xc3, 0xa9]));
	t.equal(result, '\uFFFDé', 'backtrack into valid multi-byte sequence');
});

test('TextDecoder - ArrayBuffer input', (t) => {
	const d = new TextDecoder();
	const buf = new Uint8Array([72, 105]).buffer;
	t.equal(d.decode(buf), 'Hi', 'ArrayBuffer');
});
