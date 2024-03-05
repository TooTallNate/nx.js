import { createServerHandler, createStaticFileHandler } from '@nx.js/http';
import { createRequestHandler, ServerBuild } from '@remix-run/server-runtime';

// @ts-expect-error The Remix server build does not include any types
import * as _build from '../build/server/index.js';
const build: ServerBuild = _build;

const staticHandler = createStaticFileHandler('romfs:/public/');
const requestHandler = createRequestHandler(build);

/**
 * Create a TCP echo server bound to "0.0.0.0:8080".
 */
Switch.listen({
	port: 8080,

	accept: createServerHandler(async (req) => {
		let res = await staticHandler(req);
		if (!res) res = await requestHandler(req);
		return res;
	}),
});

const { ip } = Switch.networkInfo();
console.log('TCP server listening on "%s:%d"', ip, 8080);
