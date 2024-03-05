import { createServerHandler, createStaticFileHandler } from '@nx.js/http';

const staticHandler = createStaticFileHandler('sdmc:/');

/**
 * Create a TCP echo server bound to "0.0.0.0:8080".
 */
Switch.listen({
	port: 8080,

	// `accept` is invoked when a new TCP client has connected
	accept: createServerHandler(async (req) => {
		const res = await staticHandler(req);
		if (res) return res;
		return new Response('not found', { status: 404 });
	}),
});

const { ip } = Switch.networkInfo();
console.log('TCP server listening on "%s:%d"', ip, 8080);
