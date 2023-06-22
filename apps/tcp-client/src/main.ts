import stripAnsi from 'strip-ansi';

function fdToStream(fd: number) {
	const decoder = new TextDecoder();
	const buffer = new ArrayBuffer(102400);
	let prev = '';
	return new ReadableStream<string>({
		async pull(controller) {
			const bytesRead = await Switch.read(fd, buffer);
			if (bytesRead === 0) {
				controller.close();
			} else {
				const str = prev + decoder.decode(buffer.slice(0, bytesRead));
				const lines = str.split('\r\n');
				prev = lines.pop()!;
				if (lines.length === 0) {
					return this.pull!(controller);
				}
				for (const line of lines) {
					controller.enqueue(`${line}\n`);
				}
			}
		},
	});
}

async function main() {
	// "towel.blinkenlights.nl" no longer works on IPv4, so we'll
	// connect to "telehack.com" and type the "starwars" command
	const fd = await Switch.connect({ hostname: 'telehack.com', port: 23 });
	const stream = fdToStream(fd);
	const reader = stream.getReader();

	// wait for prompt
	while (true) {
		const chunk = await reader.read();
		if (chunk.value?.includes('More commands become available')) {
			break;
		}
	}

	// write "starwars" command
	await Switch.write(fd, new TextEncoder().encode('starwars\r\n').buffer);

	// next line is the "starwars" echo, which we can ignore
	await reader.read();

	// dump the movie to the console
	let lineCount = 0;
	while (true) {
		const chunk = await reader.read();
		if (chunk.done) break;
		if (chunk.value) {
			// there's 13 lines per frame, but libnx apparently has a bug
			// with the ANSI escape sequence "\x1B[H" (with no explicit
			// row/column values), so we'll reset the frame manually,
			// and also move it down so that the frame is centered
			if (lineCount++ % 13 === 0) {
				Switch.print('\x1B[17;1H');
			}
			Switch.print(`      ${stripAnsi(chunk.value).slice(0, 67)}\n`);
		}
	}
	console.log('TCP socket closed');
}

main().catch(console.error);
