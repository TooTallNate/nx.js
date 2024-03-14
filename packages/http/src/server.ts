import { encoder } from './util';
import { bodyStream } from './body';
import { readHeaders, toHeaders } from './headers';
import type { UnshiftableStream } from './unshiftable-readable-stream';

const STATUS_CODES: Record<string, string> = {
	'100': 'Continue',
	'101': 'Switching Protocols',
	'102': 'Processing',
	'103': 'Early Hints',
	'200': 'OK',
	'201': 'Created',
	'202': 'Accepted',
	'203': 'Non-Authoritative Information',
	'204': 'No Content',
	'205': 'Reset Content',
	'206': 'Partial Content',
	'207': 'Multi-Status',
	'208': 'Already Reported',
	'226': 'IM Used',
	'300': 'Multiple Choices',
	'301': 'Moved Permanently',
	'302': 'Found',
	'303': 'See Other',
	'304': 'Not Modified',
	'305': 'Use Proxy',
	'307': 'Temporary Redirect',
	'308': 'Permanent Redirect',
	'400': 'Bad Request',
	'401': 'Unauthorized',
	'402': 'Payment Required',
	'403': 'Forbidden',
	'404': 'Not Found',
	'405': 'Method Not Allowed',
	'406': 'Not Acceptable',
	'407': 'Proxy Authentication Required',
	'408': 'Request Timeout',
	'409': 'Conflict',
	'410': 'Gone',
	'411': 'Length Required',
	'412': 'Precondition Failed',
	'413': 'Payload Too Large',
	'414': 'URI Too Long',
	'415': 'Unsupported Media Type',
	'416': 'Range Not Satisfiable',
	'417': 'Expectation Failed',
	'418': "I'm a Teapot",
	'421': 'Misdirected Request',
	'422': 'Unprocessable Entity',
	'423': 'Locked',
	'424': 'Failed Dependency',
	'425': 'Too Early',
	'426': 'Upgrade Required',
	'428': 'Precondition Required',
	'429': 'Too Many Requests',
	'431': 'Request Header Fields Too Large',
	'451': 'Unavailable For Legal Reasons',
	'500': 'Internal Server Error',
	'501': 'Not Implemented',
	'502': 'Bad Gateway',
	'503': 'Service Unavailable',
	'504': 'Gateway Timeout',
	'505': 'HTTP Version Not Supported',
	'506': 'Variant Also Negotiates',
	'507': 'Insufficient Storage',
	'508': 'Loop Detected',
	'509': 'Bandwidth Limit Exceeded',
	'510': 'Not Extended',
	'511': 'Network Authentication Required',
};

export async function readRequest(
	unshiftable: UnshiftableStream,
): Promise<Request | null> {
	const rawHeaders = await readHeaders(unshiftable);
	if (!rawHeaders.length) {
		// Malformed HTTP request - no header sent
		return null;
	}

	const [method, path, _httpVersion] = rawHeaders[0].split(' ');
	const headers = toHeaders(rawHeaders.slice(1));

	const host = headers.get('host');
	const body =
		method === 'GET' || method === 'HEAD'
			? null
			: bodyStream(unshiftable, headers);
	return new Request(`http://${host}${path}`, {
		method,
		body,
		headers,
		// @ts-expect-error wtf Node.js - https://github.com/nodejs/node/issues/46221
		duplex: 'half',
	});
}

export function createChunkedWriter(readable: ReadableStream<Uint8Array>) {
	const reader = readable.getReader();
	return new ReadableStream<Uint8Array>({
		async pull(controller) {
			const { done, value } = await reader.read();
			const bytes = done ? 0 : value.length;
			controller.enqueue(encoder.encode(`${bytes.toString(16)}\r\n`));
			if (value) controller.enqueue(value);
			controller.enqueue(encoder.encode(`\r\n`));
			if (done) controller.close();
		},
	});
}

export async function writeResponse(
	writable: WritableStream<Uint8Array>,
	res: Response,
	httpVersion: string,
): Promise<void> {
	const close = res.headers.get('connection') === 'close';
	const chunked = !(close || res.headers.has('content-length'));
	if (!res.headers.has('date')) {
		res.headers.set('date', new Date().toUTCString());
	}
	if (!res.headers.has('content-type')) {
		res.headers.set('content-type', 'text/plain');
	}
	if (chunked) {
		res.headers.set('transfer-encoding', 'chunked');
	}

	let headersStr = '';
	for (const [k, v] of res.headers) {
		headersStr += `${k}: ${v}\r\n`;
	}

	const statusText = res.statusText || STATUS_CODES[res.status];
	const header = `${httpVersion} ${res.status} ${statusText}\r\n${headersStr}\r\n`;
	const w = writable.getWriter();
	w.write(encoder.encode(header));
	w.releaseLock();

	let body = res.body;
	if (body) {
		if (chunked) body = createChunkedWriter(body);
		await body.pipeTo(writable, { preventClose: true });
	}
	if (close) {
		await writable.close();
	}
}
