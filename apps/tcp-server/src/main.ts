/**
 * Create a TCP echo server bound to "0.0.0.0:8080".
 */
Switch.listen({
	port: 8080,

	// `accept` is invoked when a new TCP client has connected
	async accept({ socket }) {
		console.log('Client connection established');
		const reader = socket.readable.getReader();
		const writer = socket.writable.getWriter();
		try {
			while (true) {
				const chunk = await reader.read();
				if (chunk.done) {
					console.log('Client closed connection');
					break;
				}

				const text = new TextDecoder().decode(chunk.value);
				console.log('Read', { text });

				// Echo the input back to the client
				await writer.write(chunk.value);
				console.log('Wrote %d bytes', chunk.value.length);
			}
		} catch (err) {
			console.error(err);
		}
	},
});

const { ip } = Switch.networkInfo();
console.log('TCP server listening on "%s:%d"', ip, 8080);
