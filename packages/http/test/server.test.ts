import { expect, test, describe } from 'vitest';
import { readRequest, writeResponse } from '../src/server';
import { UnshiftableStream } from '../src/unshiftable-readable-stream';

const decoder = new TextDecoder();
const encoder = new TextEncoder();

describe('readRequest()', () => {
	test('single request - end of stream', async () => {
		const sourceStream = new ReadableStream<Uint8Array>({
			pull(controller) {
				controller.enqueue(
					encoder.encode(
						`POST /a?b=foo HTTP/1.1\r\nHost: foo.com\r\nUser-Agent: curl/8.4.0\r\nAccept: */*\r\n\r\nbody`,
					),
				);
				controller.close();
			},
		});
		const unshiftable = new UnshiftableStream(sourceStream);
		const req = await readRequest(unshiftable);
		expect(req?.url).toEqual('http://foo.com/a?b=foo');
		expect(req?.method).toEqual('POST');
		expect(req?.headers.get('Host')).toEqual('foo.com');
		expect(await req?.text()).toEqual('body');
		expect(unshiftable.paused).toBeTruthy();
	});

	test('single request - chunked encoding', async () => {
		const sourceStream = new ReadableStream<Uint8Array>({
			pull(controller) {
				controller.enqueue(
					encoder.encode(
						`POST /a?b=foo HTTP/1.1\r\nHost: foo.com\r\nUser-Agent: curl/8.4.0\r\nAccept: */*\r\nTransfer-Encoding: chunked\r\n\r\n7\r\nMozilla\r\n11\r\nDeveloper Network\r\n0\r\n\r\nextra bytes`,
					),
				);
				controller.close();
			},
		});
		const unshiftable = new UnshiftableStream(sourceStream);
		const req = await readRequest(unshiftable);
		expect(req?.method).toEqual('POST');
		expect(req?.headers.get('Host')).toEqual('foo.com');
		expect(await req?.text()).toEqual('MozillaDeveloper Network');
		expect(unshiftable.paused).toBeTruthy();

		const reader = unshiftable.readable.getReader();
		unshiftable.resume();
		expect(decoder.decode((await reader.read()).value)).toEqual('extra bytes');
		expect((await reader.read()).done).toEqual(true);
	});

	test('single request - content-length', async () => {
		const sourceStream = new ReadableStream<Uint8Array>({
			pull(controller) {
				controller.enqueue(
					encoder.encode(
						`POST /a?b=foo HTTP/1.1\r\nHost: foo.com\r\nUser-Agent: curl/8.4.0\r\nAccept: */*\r\nContent-Length: 4\r\n\r\nbody with some extra`,
					),
				);
				controller.close();
			},
		});
		const unshiftable = new UnshiftableStream(sourceStream);
		const req = await readRequest(unshiftable);
		expect(req?.url).toEqual('http://foo.com/a?b=foo');
		expect(req?.method).toEqual('POST');
		expect(req?.headers.get('Host')).toEqual('foo.com');
		expect(await req?.text()).toEqual('body');
		expect(unshiftable.paused).toBeTruthy();

		const reader = unshiftable.readable.getReader();
		unshiftable.resume();
		expect(decoder.decode((await reader.read()).value)).toEqual(
			' with some extra',
		);
		expect((await reader.read()).done).toEqual(true);
	});
});
