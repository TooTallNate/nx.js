import mime from 'mime';

/**
 * Options object for the {@link createStaticFileHandler | `createStaticFileHandler()`} function.
 */
export interface StaticFileHandlerOpts {
	/**
	 * Addional HTTP headers to include in the response. Can be a static
	 * `Headers` object, or a function which returns a `Headers` object.
	 */
	headers?: HeadersInit | ((req: Request, path: URL) => HeadersInit);
}

export function resolvePath(url: string, root: string): URL {
	const { pathname } = new URL(url);
	const resolved = new URL(pathname.replace(/^\/*/, ''), root);
	return resolved;
}

/**
 * Creates an HTTP handler function which serves file contents from the filesystem.
 *
 * @example
 *
 * ```typescript
 * import { createStaticFileHandler, listen } from '@nx.js/http';
 *
 * const fileHandler = createStaticFileHandler('sdmc:/switch/');
 *
 * listen({
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
 * @param opts Optional options object
 */
export function createStaticFileHandler(
	root: Switch.PathLike,
	opts?: StaticFileHandlerOpts,
) {
	let rootStr = String(root);
	if (!rootStr.endsWith('/')) rootStr += '/';
	const oHeaders = opts?.headers;
	return async (req: Request): Promise<Response | null> => {
		const url = resolvePath(req.url, rootStr);
		const stat = await Switch.stat(url);
		if (!stat) return null;
		// TODO: replace with readable stream API
		const data = await Switch.readFile(url);
		if (!data) return null;
		const headers = new Headers(
			typeof oHeaders === 'function' ? oHeaders(req, url) : oHeaders,
		);
		headers.set('content-length', String(data.byteLength));
		headers.set(
			'content-type',
			mime.getType(url.pathname) || 'application/octet-stream',
		);
		return new Response(data, { headers });
	};
}
