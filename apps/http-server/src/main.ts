import * as http from '@nx.js/http';

/**
 * Create a HTTP server bound to "0.0.0.0:8080".
 */
http.listen({
	port: 8080,

	fetch(req) {
		console.log(`Got HTTP ${req.method} request for "${req.url}"`);
		const b = new ArrayBuffer(2e+6);
		console.log(`Total bytes: ${b.byteLength}`);
		return new Response(b, {
			headers: {
				"content-length": String(b.byteLength),
			},
		});
	},
});

const { ip } = Switch.networkInfo();
console.log('HTTP server listening on "%s:%d"', ip, 8080);
