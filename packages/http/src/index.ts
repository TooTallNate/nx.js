import mime from 'mime';
import { UnshiftableStream } from './unshiftable-readable-stream';
import { readRequest, writeResponse } from './server';

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
		const unshiftable = new UnshiftableStream(socket.readable);
		while (true) {
			const req = await readRequest(unshiftable);
			if (!req) break;
			const res = await handler(req);
			writeResponse(socket.writable, res, 'HTTP/1.1');
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
 *     console.log(`Got HTTP ${req.method} request for "${req.url}"`);
 *     return new Response('Hello World!');
 *   }
 * });
 * ```
 */
export function listen(opts: ListenOptions) {
	return Switch.listen({
		...opts,
		accept: createServerHandler(opts.fetch),
	});
}
