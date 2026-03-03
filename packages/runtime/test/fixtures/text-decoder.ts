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
	t.throws(() => {
		d.decode(new Uint8Array([0xff]));
	}, undefined, 'throws on invalid byte');
});

test('TextDecoder - fatal mode valid input', (t) => {
	const d = new TextDecoder('utf-8', { fatal: true });
	const encoded = new TextEncoder().encode('Hello 你好 😀');
	t.equal(d.decode(encoded), 'Hello 你好 😀', 'valid input works in fatal mode');
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

test('TextDecoder - ArrayBuffer input', (t) => {
	const d = new TextDecoder();
	const buf = new Uint8Array([72, 105]).buffer;
	t.equal(d.decode(buf), 'Hi', 'ArrayBuffer');
});
