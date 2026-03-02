import { test } from '../src/tap';

// --- Helpers ---

async function compress(format: string, data: Uint8Array): Promise<Uint8Array> {
	const cs = new CompressionStream(format as CompressionFormat);
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
	const ds = new DecompressionStream(format as CompressionFormat);
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

// --- zstd roundtrip ---

test('zstd roundtrip', async (t) => {
	const input = new TextEncoder().encode(
		'Hello, World! This is a test of zstd compression in nx.js.',
	);
	const compressed = await compress('zstd', input);
	t.ok(compressed.length > 0, 'compressed has data');
	const decompressed = await decompress('zstd', compressed);
	t.equal(
		new TextDecoder().decode(decompressed),
		'Hello, World! This is a test of zstd compression in nx.js.',
		'roundtrip matches',
	);
});

test('zstd large data roundtrip', async (t) => {
	const text = 'The quick brown fox jumps over the lazy dog. '.repeat(1000);
	const input = new TextEncoder().encode(text);
	const compressed = await compress('zstd', input);
	t.ok(compressed.length < input.length, 'zstd compression reduces size');
	const decompressed = await decompress('zstd', compressed);
	t.equal(decompressed.length, input.length, 'decompressed length matches');
	t.equal(
		new TextDecoder().decode(decompressed),
		text,
		'decompressed content matches',
	);
});
