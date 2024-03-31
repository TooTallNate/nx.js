import { expect, test } from 'vitest';
import { readHeaders, toHeaders } from '../src/headers';
import { UnshiftableStream } from '../src/unshiftable-readable-stream';

const decoder = new TextDecoder();
const encoder = new TextEncoder();

test('readHeaders()', async () => {
	const sourceStream = new ReadableStream<Uint8Array>({
		pull(controller) {
			controller.enqueue(
				encoder.encode(
					`GET / HTTP/1.1\r\nHost: foo.com\r\nUser-Agent: curl/8.4.0\r\nAccept: */*\r\n\r\nbody`,
				),
			);
			controller.close();
		},
	});
	const unshiftable = new UnshiftableStream(sourceStream);
	const headers = await readHeaders(unshiftable);
	expect(unshiftable.paused).toBeTruthy();
	expect(headers).toEqual([
		'GET / HTTP/1.1',
		'Host: foo.com',
		'User-Agent: curl/8.4.0',
		'Accept: */*',
	]);

	// Check that the body portion was unshifted
	const reader = unshiftable.readable.getReader();
	unshiftable.resume();
	expect(decoder.decode((await reader.read()).value)).toEqual('body');
	expect((await reader.read()).done).toEqual(true);
});

test('toHeaders()', () => {
	const input = ['Host: foo.com', 'User-Agent: curl/8.4.0', 'Accept: */*'];
	const headers = toHeaders(input);
	expect(headers).toBeInstanceOf(Headers);
	expect(Array.from(headers)).toEqual([
		['accept', '*/*'],
		['host', 'foo.com'],
		['user-agent', 'curl/8.4.0'],
	]);
});
