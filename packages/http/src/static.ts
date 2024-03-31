import mime from 'mime';

export interface StaticFileHandlerOpts {
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
export function createStaticFileHandler(
	root: Switch.PathLike,
	opts: StaticFileHandlerOpts = {},
) {
	let rootStr = String(root);
	if (!rootStr.endsWith('/')) rootStr += '/';
	return async (req: Request): Promise<Response | null> => {
		const url = resolvePath(req.url, rootStr);
		const stat = await Switch.stat(url);
		if (!stat) return null;
		// TODO: replace with readable stream API
		const data = await Switch.readFile(url);
		if (!data) return null;
		const headers = new Headers(
			typeof opts.headers === 'function'
				? opts.headers(req, url)
				: opts.headers,
		);
		headers.set('content-length', String(data.byteLength));
		headers.set(
			'content-type',
			mime.getType(url.pathname) || 'application/octet-stream',
		);
		return new Response(data, { headers });
	};
}
