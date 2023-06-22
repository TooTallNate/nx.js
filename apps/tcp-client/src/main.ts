let fd: number;
const buffer = new ArrayBuffer(10240);
const decoder = new TextDecoder();

async function read() {
	const bytesRead = await Switch.read(fd, buffer);
	if (bytesRead > 0) {
		//console.log({ bytesRead });
		//console.log(buffer);
		try {
			const str = decoder.decode(buffer.slice(0, bytesRead)).replace('\x1B[H', '\x1B[1;1H')
			//console.log({ str });
			Switch.print(str);
		} catch (err) {
			console.error(err);
		}
		return read();
	} else {
		console.log('TCP socket closed');
	}
}

async function main() {
	// "towel.blinkenlights.nl" no longer works on IPv4, so we'll
	// connect to "telehack.com" and type the "starwars" command
	fd = await Switch.connect({ hostname: 'telehack.com', port: 23 });
	console.log({ fd });

	read().catch(console.error);

	const a = new Uint8Array([115, 116, 97, 114, 119, 97, 114, 115, 13, 10]);
	setTimeout(() => {
		Switch.native.write(
			(err, b) => {
				console.log({ err, b });
			},
			fd,
			a.buffer
		);
	}, 2000);
}

main().catch(console.error);
