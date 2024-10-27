import { UnshiftableStream } from './unshiftable-readable-stream';
import { readRequest, writeResponse } from './server';

export {
	createStaticFileHandler,
	type StaticFileHandlerOptions,
} from './static';

/**
 * Handler function for processing an HTTP request.
 */
export type ServerHandler = (req: Request) => Response | Promise<Response>;

/**
 * Options object for the {@link listen | `listen()`} function.
 */
export interface ListenOptions extends Omit<Switch.ListenOptions, 'accept'> {
	fetch: ServerHandler;
}

/**
 * Creates a socket handler function which accepts a socket
 * event and parses the incoming data as an HTTP request.
 *
 * > NOTE: This is a low-level function which usually should not be used directly.
 * > See {@link listen | `listen()`} instead, which is a higher-level wrapper
 * > around this function.
 *
 * @param handler The HTTP handler function to invoke when an HTTP request is received
 */
export function createServerHandler(handler: ServerHandler) {
	return async (event: Switch.SocketEvent) => {
		const s = event.socket;
		const unshiftable = new UnshiftableStream(s.readable);
		while (true) {
			const req = await readRequest(unshiftable);
			if (!req) break;
			const res = await handler(req);
			writeResponse(s.writable, res, 'HTTP/1.1');
		}
		s.close();
	};
}

/**
 * Creates an HTTP server and listens on the `port` number specified in the options object.
 * The `fetch` function will be invoked upon receiving an HTTP request.
 *
 * @example
 *
 * ```typescript
 * import { listen } from '@nx.js/http';
 *
 * listen({
 *   port: 8080,
 *   fetch(req) {
 *     console.log(`Got HTTP ${req.method} request for "${req.url}"`);
 *     return new Response('Hello World!');
 *   }
 * });
 * ```
 */
export function listen(opts: ListenOptions) {
	const accept = createServerHandler(opts.fetch);
	return Switch.listen({ ...opts, accept });
}
