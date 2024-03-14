import * as http from '@nx.js/http';

/**
 * Create a HTTP server bound to "0.0.0.0:8080".
 */
http.listen({
	port: 8080,

	fetch(req) {
		console.log(`Got HTTP "${req.method}" request: ${req.url}`);
		return new Response('Hello World!');
	},
});

const { ip } = Switch.networkInfo();
console.log('HTTP server listening on "%s:%d"', ip, 8080);
