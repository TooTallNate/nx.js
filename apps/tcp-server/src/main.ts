// Create a TCP server bound to "0.0.0.0:8080".
const port = 8080;
const server = Switch.listen({ port });

// "accept" event occurs when a new TCP client has connected.
server.addEventListener('accept', async ({ fd }) => {
	try {
		console.log('Client connection established (fd=%d)', fd);
		const buf = new ArrayBuffer(1024);
		while (1) {
			let bytes = await Switch.read(fd, buf);
			if (bytes === 0) {
				console.log('Client connection closed (fd=%d)', fd);
				break;
			}
			const arr = new Uint8Array(buf, 0, bytes);
			const text = new TextDecoder().decode(arr);
			console.log('Read', { fd, text, bytes });

			// Echo the input back to the client
			bytes = await Switch.write(fd, arr);
			console.log('Write', { fd, bytes });
		}
	} catch (err) {
		console.error(err);
	}
});

console.log('TCP server listening on port %d', port);
