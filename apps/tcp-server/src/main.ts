/**
 * Create a TCP echo server bound to "0.0.0.0:8080".
 */
const port = 8080;
const server = Switch.listen({ port });

// "accept" event occurs when a new TCP client has connected
server.addEventListener('accept', async ({ socket }) => {
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
});

const { ip } = Switch.networkInfo();
console.log('TCP server listening on "%s:%d"', ip, port);
