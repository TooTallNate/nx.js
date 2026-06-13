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

// --- streaming pipe through pipeThrough (regression for the applet-mode
// native-heap back-pressure fix) ---
//
// Reading a DecompressionStream chunk-by-chunk via pipeThrough is the exact
// path that, on a memory-constrained device, accumulated external ArrayBuffer
// backing stores faster than V8 collected them (until the runtime began
// firing MemoryPressureNotification under native-heap pressure). This can't
// reproduce the OOM on the host (plenty of RAM), but it locks in that the
// pipe yields byte-exact output — the functional contract the GC nudge must
// not regress. (Chunk count is implementation-defined — the host may emit the
// whole stream in one chunk while the device emits many — so this only asserts
// byte-exactness, not the chunk count.)
test('zstd streaming pipeThrough roundtrip', async (t) => {
	// ~8 MB of semi-compressible data, exercising a long decompression stream.
	const block = new Uint8Array(64 * 1024);
	for (let i = 0; i < block.length; i++) block[i] = (i * 31 + 7) & 0xff;
	const blocks: Uint8Array[] = [];
	let total = 0;
	for (let i = 0; i < 128; i++) {
		blocks.push(block);
		total += block.length;
	}

	const compressed = await new Response(
		new Blob(blocks).stream().pipeThrough(new CompressionStream('zstd')),
	).arrayBuffer();
	t.ok(compressed.byteLength > 0, 'compressed has data');

	// Decompress via pipeThrough and verify length + a content checksum.
	const ds = new Blob([compressed])
		.stream()
		.pipeThrough(new DecompressionStream('zstd'));
	const reader = ds.getReader();
	let outLen = 0;
	let phase = 0;
	let mismatch = -1;
	while (true) {
		const { done, value } = await reader.read();
		if (done) break;
		// Verify the byte pattern is preserved across chunk boundaries.
		for (let i = 0; i < value.length; i++) {
			const expected = (phase * 31 + 7) & 0xff;
			if (value[i] !== expected && mismatch < 0) {
				mismatch = outLen + i;
			}
			phase = (phase + 1) % block.length;
		}
		outLen += value.length;
	}
	t.equal(outLen, total, 'streamed decompressed length matches');
	t.equal(mismatch, -1, 'streamed bytes match the source pattern exactly');
});

// --- fused native file decompression fast path ---
//
// `Switch.file(path).stream().pipeThrough(new DecompressionStream(fmt))` is
// transparently routed to a fused native read+decompress op (see
// compression-streams.ts / polyfills/streams.ts). This test exercises that
// real path on nxjs-test (which has Switch.file), writing a compressed temp
// file and decompressing it back. On Bun/Chrome (no Switch.file) it runs the
// equivalent in-memory Blob path, so both environments produce identical TAP
// — locking in that the fused fast path yields byte-exact output.
test('zstd file.stream().pipeThrough fused path roundtrip', async (t) => {
	const N = 2 * 1024 * 1024; // 2 MB, enough to span many fused chunks
	const src = new Uint8Array(N);
	for (let i = 0; i < N; i++) src[i] = (i * 131 + 17) & 0xff;

	const compressed = new Uint8Array(
		await new Response(
			new Blob([src]).stream().pipeThrough(new CompressionStream('zstd')),
		).arrayBuffer(),
	);

	// Build the input stream: native file source when available, else in-memory.
	const Sw: any = (globalThis as any).Switch;
	let stream: ReadableStream<Uint8Array>;
	let tmp: string | undefined;
	if (Sw && typeof Sw.file === 'function' && typeof Sw.writeFileSync === 'function') {
		tmp = 'nxjs-fused-zstd.tmp';
		Sw.writeFileSync(tmp, compressed);
		stream = Sw.file(tmp).stream();
	} else {
		stream = new Blob([compressed]).stream();
	}

	const out = new Uint8Array(
		await new Response(
			stream.pipeThrough(new DecompressionStream('zstd')),
		).arrayBuffer(),
	);

	t.equal(out.length, N, 'fused decompressed length matches source');
	let mismatch = -1;
	for (let i = 0; i < out.length; i++) {
		if (out[i] !== ((i * 131 + 17) & 0xff)) { mismatch = i; break; }
	}
	t.equal(mismatch, -1, 'fused decompressed bytes are byte-exact');

	if (tmp && Sw && typeof Sw.remove === 'function') {
		try { Sw.remove(tmp); } catch {}
	}
});
