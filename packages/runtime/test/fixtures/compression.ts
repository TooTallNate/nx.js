import { test } from '../src/tap';

// Uint8Array hex methods (TC39 stage 4, supported in all target runtimes)
declare global {
	interface Uint8Array {
		toHex(): string;
	}
	interface Uint8ArrayConstructor {
		fromHex(hex: string): Uint8Array;
	}
}

// --- Helpers ---

async function compress(
	format: CompressionFormat,
	data: Uint8Array,
): Promise<Uint8Array> {
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
	format: CompressionFormat,
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

async function compressPipeThrough(
	format: CompressionFormat,
	input: Uint8Array,
): Promise<Uint8Array> {
	const r = new ReadableStream({
		start(controller) {
			controller.enqueue(input);
			controller.close();
		},
	});
	const cs = new CompressionStream(format);
	r.pipeThrough(cs);
	const res = new Response(cs.readable);
	return new Uint8Array(await res.arrayBuffer());
}

async function decompressPipeThrough(
	format: CompressionFormat,
	input: Uint8Array,
): Promise<string> {
	const r = new ReadableStream({
		start(controller) {
			controller.enqueue(input);
			controller.close();
		},
	});
	const ds = new DecompressionStream(format);
	r.pipeThrough(ds);
	const res = new Response(ds.readable);
	return res.text();
}

// --- Roundtrip tests (writer/reader pattern) ---

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

// --- Large data roundtrip (all standard formats) ---

test('large data roundtrip - gzip', async (t) => {
	const text = 'The quick brown fox jumps over the lazy dog. '.repeat(1000);
	const input = new TextEncoder().encode(text);
	const compressed = await compress('gzip', input);
	t.ok(compressed.length < input.length, 'gzip compression reduces size');
	const decompressed = await decompress('gzip', compressed);
	t.equal(decompressed.length, input.length, 'decompressed length matches');
	t.equal(
		new TextDecoder().decode(decompressed),
		text,
		'decompressed content matches',
	);
});

test('large data roundtrip - deflate', async (t) => {
	const text = 'The quick brown fox jumps over the lazy dog. '.repeat(1000);
	const input = new TextEncoder().encode(text);
	const compressed = await compress('deflate', input);
	t.ok(compressed.length < input.length, 'deflate compression reduces size');
	const decompressed = await decompress('deflate', compressed);
	t.equal(
		new TextDecoder().decode(decompressed),
		text,
		'decompressed content matches',
	);
});

test('large data roundtrip - deflate-raw', async (t) => {
	const text = 'The quick brown fox jumps over the lazy dog. '.repeat(1000);
	const input = new TextEncoder().encode(text);
	const compressed = await compress('deflate-raw', input);
	t.ok(
		compressed.length < input.length,
		'deflate-raw compression reduces size',
	);
	const decompressed = await decompress('deflate-raw', compressed);
	t.equal(
		new TextDecoder().decode(decompressed),
		text,
		'decompressed content matches',
	);
});

// --- Decompression from known bytes ---

test('DecompressionStream - deflate from known bytes', async (t) => {
	const text = await decompressPipeThrough(
		'deflate',
		Uint8Array.fromHex('789cf348cdc9c95708cf2fca490100180b041d'),
	);
	t.equal(text, 'Hello World', 'decompressed deflate matches');
});

test('DecompressionStream - deflate-raw from known bytes', async (t) => {
	const text = await decompressPipeThrough(
		'deflate-raw',
		Uint8Array.fromHex('f348cdc9c95708cf2fca490100'),
	);
	t.equal(text, 'Hello World', 'decompressed deflate-raw matches');
});

test('DecompressionStream - gzip from known bytes', async (t) => {
	const text = await decompressPipeThrough(
		'gzip',
		Uint8Array.fromHex(
			'1f8b0800315272670003f348cdc9c95708cf2fca49010056b1174a0b000000',
		),
	);
	t.equal(text, 'Hello World', 'decompressed gzip matches');
});

// --- CompressionStream with pipeThrough + Response pattern ---

test('CompressionStream - deflate-raw via pipeThrough', async (t) => {
	const input = new TextEncoder().encode('Hello World');
	const data = await compressPipeThrough('deflate-raw', input);
	t.equal(
		data.toHex(),
		'f348cdc9c95708cf2fca490100',
		'deflate-raw compressed output matches expected bytes',
	);
});

test('CompressionStream - deflate via pipeThrough', async (t) => {
	const input = new TextEncoder().encode('Hello World');
	const data = await compressPipeThrough('deflate', input);
	t.ok(data.length > 0, 'compressed data is not empty');
	// Verify the compressed data can be decompressed back
	const text = await decompressPipeThrough('deflate', data);
	t.equal(text, 'Hello World', 'deflate pipeThrough roundtrip matches');
});

test('CompressionStream - gzip via pipeThrough', async (t) => {
	const input = new TextEncoder().encode('Hello World');
	const data = await compressPipeThrough('gzip', input);
	t.ok(data.length > 0, 'compressed data is not empty');
	// Verify the compressed data can be decompressed back
	const text = await decompressPipeThrough('gzip', data);
	t.equal(text, 'Hello World', 'gzip pipeThrough roundtrip matches');
});

// --- Cross-engine interop (compress here, other engine decompresses) ---

test('interop - deflate compress then decompress', async (t) => {
	const input = new TextEncoder().encode(
		'The quick brown fox jumps over the lazy dog',
	);
	const compressed = await compress('deflate', input);
	t.ok(compressed.length > 0, 'compressed has data');
	const decompressed = await decompress('deflate', compressed);
	t.equal(
		new TextDecoder().decode(decompressed),
		'The quick brown fox jumps over the lazy dog',
		'deflate interop roundtrip matches',
	);
});

test('interop - gzip compress then decompress', async (t) => {
	const input = new TextEncoder().encode(
		'The quick brown fox jumps over the lazy dog',
	);
	const compressed = await compress('gzip', input);
	t.ok(compressed.length > 0, 'compressed has data');
	const decompressed = await decompress('gzip', compressed);
	t.equal(
		new TextDecoder().decode(decompressed),
		'The quick brown fox jumps over the lazy dog',
		'gzip interop roundtrip matches',
	);
});
