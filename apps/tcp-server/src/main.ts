// Create a TCP server bound to "0.0.0.0:8080".
const port = 8080;
const server = Switch.listen({ port });

// "accept" event occurs when a new TCP client has connected.
server.addEventListener('accept', async ({ fd }) => {
	console.log('Client connection established (fd=%d)', fd);
	const buf = new ArrayBuffer(10);
	while (1) {
		const n = await Switch.read(fd, buf);
		if (n === 0) {
			console.log('Client connection closed (fd=%d)', fd);
			break;
		}
		const text = new TextDecoder().decode(new Uint8Array(buf, 0, n));
		console.log('Read %d bytes (fd=%d): %j', n, fd, text);

		// Echo the input back to the client
		await Switch.write(fd, new Uint8Array(buf, 0, n));
	}
});

console.log('TCP server listening on port %d', port);
