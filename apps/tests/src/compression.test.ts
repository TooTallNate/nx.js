import { suite } from 'uvu';
import * as assert from 'uvu/assert';
import { distance } from 'fastest-levenshtein';
import { fromHex, toHex } from './util.ts';

const test = suite('compression');

if (process.env.NXJS !== '1') {
	// Install polyfills for Bun / Node.js
	(globalThis as any).CompressionStream = class extends (
		TransformStream<Uint8Array, Uint8Array>
	) {
		constructor(format: CompressionFormat) {
			let s: any;
			let end = Promise.withResolvers<void>();
			super({
				async start(controller) {
					const { createGzip, createDeflate, createDeflateRaw } = await import(
						// @ts-expect-error Not using `@types/node` so "node:zlib" is not available
						'node:zlib'
					);
					if (format === 'deflate') {
						s = createDeflate({ level: 8 });
					} else if (format === 'deflate-raw') {
						s = createDeflateRaw({ level: 8 });
					} else {
						s = createGzip({ level: 8 });
					}
					s.on('data', (chunk: Uint8Array) => {
						controller.enqueue(chunk);
					});
					s.on('end', () => {
						end.resolve();
					});
				},
				async transform(chunk) {
					await new Promise((r) => s.write(chunk, r));
				},
				async flush() {
					await new Promise((r) => s.end(r));
					await end.promise;
				},
			});
		}
	};

	(globalThis as any).DecompressionStream = class extends (
		TransformStream<Uint8Array, Uint8Array>
	) {
		constructor(format: CompressionFormat) {
			let s: any;
			let end = Promise.withResolvers<void>();
			super({
				async start(controller) {
					const { createGunzip, createInflate, createInflateRaw } =
						// @ts-expect-error Not using `@types/node` so "node:zlib" is not available
						await import('node:zlib');
					if (format === 'deflate') {
						s = createInflate();
					} else if (format === 'deflate-raw') {
						s = createInflateRaw();
					} else {
						s = createGunzip();
					}
					s.on('data', (chunk: Uint8Array) => {
						controller.enqueue(chunk);
					});
					s.on('end', () => {
						end.resolve();
					});
				},
				async transform(chunk) {
					await new Promise((r) => s.write(chunk, r));
				},
				async flush() {
					await new Promise((r) => s.end(r));
					await end.promise;
				},
			});
		}
	};
}

test('CompressionStream - deflate', async () => {
	const r = new ReadableStream({
		start(controller) {
			controller.enqueue(new TextEncoder().encode('Hello World'));
			controller.close();
		},
	});

	const d = new CompressionStream('deflate');
	r.pipeThrough(d);

	const res = new Response(d.readable);
	const data = await res.arrayBuffer();

	// Different runtimes have slightly different compression results.
	// Allow for a few bytes of difference.
	const dist = distance('789cf348cdc9c95708cf2fca490100180b041d', toHex(data));
	assert.ok(dist <= 2);
});

test('CompressionStream - deflate-raw', async () => {
	const r = new ReadableStream({
		start(controller) {
			controller.enqueue(new TextEncoder().encode('Hello World'));
			controller.close();
		},
	});

	const d = new CompressionStream('deflate-raw');
	r.pipeThrough(d);

	const res = new Response(d.readable);
	const data = await res.arrayBuffer();
	assert.equal(toHex(data), 'f348cdc9c95708cf2fca490100');
});

test('CompressionStream - gzip', async () => {
	const r = new ReadableStream({
		start(controller) {
			controller.enqueue(new TextEncoder().encode('Hello World'));
			controller.close();
		},
	});

	const d = new CompressionStream('gzip');
	r.pipeThrough(d);

	const res = new Response(d.readable);
	const data = await res.arrayBuffer();

	// Different runtimes have slightly different compression results.
	// Allow for a few bytes of difference.
	const dist = distance(
		'1f8b0800000000000013f348cdc9c95708cf2fca49010056b1174a0b000000',
		toHex(data),
	);
	assert.ok(dist <= 2);
});

test('DecompressionStream - deflate', async () => {
	const data = fromHex('789cf348cdc9c95708cf2fca490100180b041d');

	const r = new ReadableStream({
		start(controller) {
			controller.enqueue(new Uint8Array(data));
			controller.close();
		},
	});

	const d = new DecompressionStream('deflate');
	r.pipeThrough(d);

	const res = new Response(d.readable);
	const text = await res.text();
	assert.equal(text, 'Hello World');
});

test('DecompressionStream - deflate-raw', async () => {
	const data = fromHex('f348cdc9c95708cf2fca490100');

	const r = new ReadableStream({
		start(controller) {
			controller.enqueue(new Uint8Array(data));
			controller.close();
		},
	});

	const d = new DecompressionStream('deflate-raw');
	r.pipeThrough(d);

	const res = new Response(d.readable);
	const text = await res.text();
	assert.equal(text, 'Hello World');
});

test('DecompressionStream - gzip', async () => {
	const data = fromHex(
		'1f8b0800315272670003f348cdc9c95708cf2fca49010056b1174a0b000000',
	);

	const r = new ReadableStream({
		start(controller) {
			controller.enqueue(new Uint8Array(data));
			controller.close();
		},
	});

	const d = new DecompressionStream('gzip');
	r.pipeThrough(d);

	const res = new Response(d.readable);
	const text = await res.text();
	assert.equal(text, 'Hello World');
});

// Non-standard APIs that are only going to work on nx.js
if (process.env.NXJS === '1') {
	test('CompressionStream - zstd', async () => {
		const r = new ReadableStream({
			start(controller) {
				controller.enqueue(new TextEncoder().encode('Hello World'));
				controller.close();
			},
		});

		const d = new CompressionStream('zstd');
		r.pipeThrough(d);

		const res = new Response(d.readable);
		const data = await res.arrayBuffer();
		assert.equal(toHex(data), '28b52ffd005859000048656c6c6f20576f726c64');
	});

	test('DecompressionStream - zstd', async () => {
		const data = fromHex('28b52ffd045859000048656c6c6f20576f726c64c25b2419');

		const r = new ReadableStream({
			start(controller) {
				controller.enqueue(new Uint8Array(data));
				controller.close();
			},
		});

		const d = new DecompressionStream('zstd');
		r.pipeThrough(d);

		const res = new Response(d.readable);
		const text = await res.text();
		assert.equal(text, 'Hello World');
	});
}

test.run();
