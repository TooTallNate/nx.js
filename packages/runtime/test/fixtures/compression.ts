import { test } from '../src/tap';

async function compress(format: string, data: Uint8Array): Promise<Uint8Array> {
	const cs = new CompressionStream(format);
	const writer = cs.writable.getWriter();
	const reader = cs.readable.getReader();
	writer.write(data);
	writer.close();
	const chunks: Uint8Array[] = [];
	while (true) {
		const { done, value } = await reader.read();
		if (done) break;
		chunks.push(value);
	}
	let len = 0;
	for (const c of chunks) len += c.length;
	const result = new Uint8Array(len);
	let offset = 0;
	for (const c of chunks) {
		result.set(c, offset);
		offset += c.length;
	}
	return result;
}

async function decompress(
	format: string,
	data: Uint8Array,
): Promise<Uint8Array> {
	const ds = new DecompressionStream(format);
	const writer = ds.writable.getWriter();
	const reader = ds.readable.getReader();
	writer.write(data);
	writer.close();
	const chunks: Uint8Array[] = [];
	while (true) {
		const { done, value } = await reader.read();
		if (done) break;
		chunks.push(value);
	}
	let len = 0;
	for (const c of chunks) len += c.length;
	const result = new Uint8Array(len);
	let offset = 0;
	for (const c of chunks) {
		result.set(c, offset);
		offset += c.length;
	}
	return result;
}

test('gzip roundtrip', async (t) => {
	const input = new TextEncoder().encode('Hello, World!');
	const compressed = await compress('gzip', input);
	t.ok(compressed.length > 0, 'compressed has data');
	t.ok(compressed.length !== input.length, 'compressed differs from input');
	const decompressed = await decompress('gzip', compressed);
	t.equal(
		new TextDecoder().decode(decompressed),
		'Hello, World!',
		'roundtrip matches',
	);
});

test('deflate roundtrip', async (t) => {
	const input = new TextEncoder().encode('Test string for deflate compression');
	const compressed = await compress('deflate', input);
	t.ok(compressed.length > 0, 'compressed has data');
	const decompressed = await decompress('deflate', compressed);
	t.equal(
		new TextDecoder().decode(decompressed),
		'Test string for deflate compression',
		'roundtrip matches',
	);
});

test('deflate-raw roundtrip', async (t) => {
	const input = new TextEncoder().encode('Raw deflate test data');
	const compressed = await compress('deflate-raw', input);
	t.ok(compressed.length > 0, 'compressed has data');
	const decompressed = await decompress('deflate-raw', compressed);
	t.equal(
		new TextDecoder().decode(decompressed),
		'Raw deflate test data',
		'roundtrip matches',
	);
});

test('gzip large data roundtrip', async (t) => {
	const text = 'The quick brown fox jumps over the lazy dog. '.repeat(1000);
	const input = new TextEncoder().encode(text);
	const compressed = await compress('gzip', input);
	t.ok(compressed.length < input.length, 'compression reduces size');
	const decompressed = await decompress('gzip', compressed);
	t.equal(decompressed.length, input.length, 'decompressed length matches');
	t.equal(
		new TextDecoder().decode(decompressed),
		text,
		'decompressed content matches',
	);
});
