import stripAnsi from 'strip-ansi';

const fontData = Switch.readFileSync(
	new URL('GeistMono-Regular.otf', Switch.entrypoint)
);
const font = new FontFace('Geist Mono', fontData);
Switch.fonts.add(font);

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
		if (chunk.value?.includes('Press control-C')) {
			break;
		}
	}

	// write "starwars" command
	await Switch.write(fd, 'starwars\r\n');

	// next line is the "starwars" echo, which we can ignore
	await reader.read();

	const ctx = Switch.screen.getContext('2d');
	const fontSize = 30.83;
	const yOffset = 180;
	ctx.font = `${fontSize}px "Geist Mono"`;

	// dump the movie to the console
	let lineCount = 0;
	while (true) {
		const chunk = await reader.read();
		if (chunk.done) break;
		if (chunk.value) {
			const line = lineCount++ % 13;
			// there's 13 lines per frame, so when the line is
			// zero then clear the screen to begin drawing the
			// new frame
			if (line === 0) {
				ctx.fillStyle = 'black';
				ctx.fillRect(0, 0, Switch.screen.width, Switch.screen.height);
				ctx.fillStyle = 'white';
			}
			ctx.fillText(
				stripAnsi(chunk.value).slice(0, 67),
				0,
				line * fontSize + yOffset
			);
		}
	}
	console.log('TCP socket closed');
}

main().catch(console.error);
