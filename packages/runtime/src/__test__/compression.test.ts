import { describe, it, expect } from 'vitest';
import { distance } from 'fastest-levenshtein';
import { fromHex, toHex } from './util';

describe('compression', () => {
	it('CompressionStream - deflate', async () => {
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

		const dist = distance('789cf348cdc9c95708cf2fca490100180b041d', toHex(data));
		expect(dist).toBeLessThanOrEqual(2);
	});

	it('CompressionStream - deflate-raw', async () => {
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
		expect(toHex(data)).toBe('f348cdc9c95708cf2fca490100');
	});

	it('CompressionStream - gzip', async () => {
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

		const dist = distance(
			'1f8b0800000000000013f348cdc9c95708cf2fca49010056b1174a0b000000',
			toHex(data),
		);
		expect(dist).toBeLessThanOrEqual(2);
	});

	it('DecompressionStream - deflate', async () => {
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
		expect(text).toBe('Hello World');
	});

	it('DecompressionStream - deflate-raw', async () => {
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
		expect(text).toBe('Hello World');
	});

	it('DecompressionStream - gzip', async () => {
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
		expect(text).toBe('Hello World');
	});
});
