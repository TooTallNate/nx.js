import { test } from '../src/tap';

// ---------- Construction ----------

test('Blob constructor - empty', (t) => {
	const b = new Blob();
	t.equal(b.size, 0, 'empty blob has size 0');
	t.equal(b.type, '', 'empty blob has empty type');
});

test('Blob constructor - from string', (t) => {
	const b = new Blob(['hello']);
	t.equal(b.size, 5, 'string blob has correct size');
	t.equal(b.type, '', 'type defaults to empty');
});

test('Blob constructor - from multiple strings', (t) => {
	const b = new Blob(['hello', ' ', 'world']);
	t.equal(b.size, 11, 'concatenated size is correct');
});

test('Blob constructor - with type option', (t) => {
	const b = new Blob(['{}'], { type: 'application/json' });
	t.equal(b.type, 'application/json', 'type is set');
});

test('Blob constructor - type is lowercased', (t) => {
	const b = new Blob([], { type: 'Text/Plain' });
	t.equal(b.type, 'text/plain', 'type should be lowercased');
});

test('Blob constructor - from ArrayBuffer', (t) => {
	const buf = new ArrayBuffer(8);
	const b = new Blob([buf]);
	t.equal(b.size, 8, 'ArrayBuffer blob has correct size');
});

test('Blob constructor - from Uint8Array', (t) => {
	const arr = new Uint8Array([1, 2, 3, 4]);
	const b = new Blob([arr]);
	t.equal(b.size, 4, 'Uint8Array blob has correct size');
});

test('Blob constructor - from another Blob', (t) => {
	const b1 = new Blob(['hello']);
	const b2 = new Blob([b1]);
	t.equal(b2.size, 5, 'blob from blob has correct size');
});

test('Blob constructor - mixed parts', (t) => {
	const b = new Blob(['hello', new Uint8Array([32]), 'world']);
	t.equal(b.size, 11, 'mixed parts have correct total size');
});

// ---------- text() ----------

test('Blob text - returns string content', async (t) => {
	const b = new Blob(['hello world']);
	const text = await b.text();
	t.equal(text, 'hello world', 'text() returns correct string');
});

test('Blob text - concatenated parts', async (t) => {
	const b = new Blob(['foo', 'bar', 'baz']);
	const text = await b.text();
	t.equal(text, 'foobarbaz', 'text() concatenates all parts');
});

test('Blob text - empty blob', async (t) => {
	const b = new Blob();
	const text = await b.text();
	t.equal(text, '', 'empty blob text is empty string');
});

// ---------- arrayBuffer() ----------

test('Blob arrayBuffer - returns correct bytes', async (t) => {
	const b = new Blob([new Uint8Array([0xde, 0xad, 0xbe, 0xef])]);
	const buf = await b.arrayBuffer();
	const arr = new Uint8Array(buf);
	t.equal(arr.length, 4, 'arrayBuffer has correct length');
	t.equal(arr[0], 0xde, 'first byte correct');
	t.equal(arr[1], 0xad, 'second byte correct');
	t.equal(arr[2], 0xbe, 'third byte correct');
	t.equal(arr[3], 0xef, 'fourth byte correct');
});

test('Blob arrayBuffer - string content', async (t) => {
	const b = new Blob(['AB']);
	const buf = await b.arrayBuffer();
	const arr = new Uint8Array(buf);
	t.equal(arr[0], 0x41, 'A = 0x41');
	t.equal(arr[1], 0x42, 'B = 0x42');
});

// ---------- slice() ----------

test('Blob slice - basic slice', async (t) => {
	const b = new Blob(['hello world']);
	const sliced = b.slice(6, 11);
	t.equal(sliced.size, 5, 'sliced blob has correct size');
	const text = await sliced.text();
	t.equal(text, 'world', 'sliced content is correct');
});

test('Blob slice - from start', async (t) => {
	const b = new Blob(['hello world']);
	const sliced = b.slice(0, 5);
	const text = await sliced.text();
	t.equal(text, 'hello', 'slice from start');
});

test('Blob slice - negative start', async (t) => {
	const b = new Blob(['hello world']);
	const sliced = b.slice(-5);
	const text = await sliced.text();
	t.equal(text, 'world', 'negative start slices from end');
});

test('Blob slice - no arguments returns full copy', async (t) => {
	const b = new Blob(['hello']);
	const sliced = b.slice();
	t.equal(sliced.size, 5, 'slice() with no args has same size');
	const text = await sliced.text();
	t.equal(text, 'hello', 'slice() with no args has same content');
});

test('Blob slice - with content type', (t) => {
	const b = new Blob(['data'], { type: 'text/plain' });
	const sliced = b.slice(0, 4, 'application/octet-stream');
	t.equal(
		sliced.type,
		'application/octet-stream',
		'slice preserves given type',
	);
});

test('Blob slice - empty range', (t) => {
	const b = new Blob(['hello']);
	const sliced = b.slice(3, 3);
	t.equal(sliced.size, 0, 'slice with equal start/end has size 0');
});

test('Blob slice - across multiple parts', async (t) => {
	const b = new Blob(['abc', 'def', 'ghi']);
	const sliced = b.slice(2, 7);
	const text = await sliced.text();
	t.equal(text, 'cdefg', 'slice across parts returns correct content');
});

// ---------- stream() ----------

test('Blob stream - reads all bytes', async (t) => {
	const b = new Blob(['stream test']);
	const reader = b.stream().getReader();
	const chunks: Uint8Array[] = [];
	while (true) {
		const { done, value } = await reader.read();
		if (done) break;
		chunks.push(value);
	}
	const total = chunks.reduce((sum, c) => sum + c.length, 0);
	t.equal(total, b.size, 'stream yields all bytes');
});

// ---------- instanceof ----------

test('Blob instanceof', (t) => {
	const b = new Blob();
	t.ok(b instanceof Blob, 'should be instance of Blob');
});

// ---------- Immutability ----------

test('Blob is immutable - size and type are readonly', (t) => {
	const b = new Blob(['test'], { type: 'text/plain' });
	t.equal(b.size, 4, 'initial size is 4');
	t.equal(b.type, 'text/plain', 'initial type is text/plain');
	// These should be no-ops or throw in strict mode
	try {
		(b as any).size = 999;
	} catch {}
	try {
		(b as any).type = 'fake';
	} catch {}
	t.equal(b.size, 4, 'size unchanged after attempted mutation');
	t.equal(b.type, 'text/plain', 'type unchanged after attempted mutation');
});
