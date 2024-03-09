import mime from 'mime';

/**
 * Handler function for processing an HTTP request.
 */
export type ServerHandler = (req: Request) => Response | Promise<Response>;

/**
 * Options object for the {@link listen | `http.listen()`} function.
 */
export interface ListenOptions extends Omit<Switch.ListenOptions, 'accept'> {
	fetch: ServerHandler;
}

const decoder = new TextDecoder();
const encoder = new TextEncoder();

function indexOfEol(arr: ArrayLike<number>, offset: number): number {
	for (let i = offset; i < arr.length - 1; i++) {
		if (arr[i] === 13 && arr[i + 1] === 10) {
			return i;
		}
	}
	return -1;
}

function concat(a: Uint8Array, b: Uint8Array) {
	const c = new Uint8Array(a.length + b.length);
	c.set(a, 0);
	c.set(b, a.length);
	return c;
}

async function* headersIterator(
	reader: ReadableStreamDefaultReader<Uint8Array>,
): AsyncGenerator<{ line: string } | { line: null; leftover: Uint8Array }> {
	let leftover: Uint8Array | null = null;
	while (true) {
		const next = await reader.read();
		if (next.done) throw new Error('Stream closed before end of headers');
		const chunk: Uint8Array = leftover
			? concat(leftover, next.value)
			: next.value;
		let pos = 0;
		while (true) {
			const eol = indexOfEol(chunk, pos);
			if (eol === -1) {
				leftover = chunk.slice(pos);
				break;
			}
			const line = decoder.decode(chunk.slice(pos, eol));
			pos = eol + 2;
			if (line) {
				yield { line };
			} else {
				// end of headers
				leftover = chunk.slice(pos);
				yield { line: null, leftover };
				reader.releaseLock();
				return;
			}
		}
	}
}

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

/**
 * Creates a socket handler function which accepts a socket
 * event and parses the incoming data as an HTTP request.
 *
 * @note This is a low-level function which usually should not be used directly. See {@link listen | `http.listen()`} instead.
 *
 * @param handler The HTTP handler function to invoke when an HTTP request is received
 */
export function createServerHandler(handler: ServerHandler) {
	return async function ({ socket }: Switch.SocketEvent) {
		const reqHeaders = new Headers();
		const r = socket.readable.getReader();

		const hi = headersIterator(r);

		// Parse request headers
		const firstLine = await hi.next();
		if (firstLine.done || !firstLine.value.line) {
			throw new Error('Failed to read response header');
		}
		const [method, path, httpVersion] = firstLine.value.line.split(' ');

		let leftover: Uint8Array | undefined;
		for await (const v of hi) {
			if (typeof v.line === 'string') {
				const col = v.line.indexOf(':');
				const name = v.line.slice(0, col);
				const value = v.line.slice(col + 1).trim();
				reqHeaders.set(name, value);
			} else {
				leftover = v.leftover;
			}
		}

		const host = reqHeaders.get('host');
		const req = new Request(`http://${host}${path}`, {
			method,
			headers: reqHeaders,
		});
		const res = await handler(req);
		res.headers.set('connection', 'close');

		let headersStr = '';
		for (const [k, v] of res.headers) {
			headersStr += `${k}: ${v}\r\n`;
		}

		const statusText = res.statusText || STATUS_CODES[res.status];
		const resHeader = `${httpVersion} ${res.status} ${statusText}\r\n${headersStr}\r\n`;
		const w = socket.writable.getWriter();
		w.write(encoder.encode(resHeader));

		if (res.body) {
			w.releaseLock();
			await res.body.pipeTo(socket.writable);
		} else {
			await w.close();
		}

		socket.close();
	};
}

/**
 * Creates an HTTP handler function which serves file contents from the filesystem.
 *
 * @example
 *
 * ```typescript
 * const fileHandler = http.createStaticFileHandler('sdmc:/switch/');
 *
 * http.listen({
 *   port: 8080,
 *   async fetch(req) {
 *     let res = await fileHandler(req);
 *     if (!res) {
 *       res = new Response('File not found', { status: 404 });
 *     }
 *     return res;
 *   }
 * });
 * ```
 *
 * @param root Root directory where static files are served from
 */
export function createStaticFileHandler(root: Switch.PathLike) {
	if (!String(root).endsWith('/')) {
		throw new Error('`root` directory must end with a "/"');
	}
	return async function (req: Request): Promise<Response | null> {
		const { pathname } = new URL(req.url);
		const url = new URL(pathname.slice(1), root);
		const stat = await Switch.stat(url);
		if (!stat) return null;
		// TODO: replace with readable stream API
		const data = await Switch.readFile(url);
		if (!data) return null;
		return new Response(data, {
			headers: {
				'content-length': String(data.byteLength),
				'content-type': mime.getType(pathname) || 'application/octet-stream',
			},
		});
	};
}

/**
 * Creates an HTTP server and listens on the `port` number specified in the options object.
 * The `fetch` function will be invoked upon receiving an HTTP request.
 *
 * @example
 *
 * ```typescript
 * http.listen({
 *   port: 8080,
 *   fetch(req) {
 *     console.log(`Got HTTP ${req.method} request for "${req.url}`");
 *     return new Response('Hello World!');
 *   }
 * });
 * ```
 */
export function listen(opts: ListenOptions) {
	const handler = createServerHandler(opts.fetch);
	return Switch.listen({
		...opts,
		accept: handler,
	});
}
