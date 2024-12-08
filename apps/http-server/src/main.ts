import * as http from '@nx.js/http';

/**
 * Create an HTTP server bound to port 80.
 */
const port = 80;

http.listen({
	port,

	fetch(req) {
		console.log(`Got HTTP "${req.method}" request: ${req.url}`);
		return new Response('Hello World!');
	},
});

const { ip } = Switch.networkInfo();
console.log('HTTP server listening on "%s:%d"', ip, port);
