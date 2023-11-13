import stripAnsi from 'strip-ansi';
import { lineIterator } from './line-iterator';

const LINES_PER_FRAME = 13;
const decoder = new TextDecoder();
const encoder = new TextEncoder();

// Register "Geist Mono" font
const fontUrl = new URL('fonts/GeistMono-Regular.otf', Switch.entrypoint);
const fontData = Switch.readFileSync(fontUrl);
const font = new FontFace('Geist Mono', fontData);
Switch.fonts.add(font);

async function* frameIterator(
	readable: ReadableStreamDefaultReader<Uint8Array>
) {
	const it = lineIterator(readable);

	// first line is the "starwars" echo, which we can ignore
	await it.next();

	const lines: string[] = [];
	for await (const line of it) {
		lines.push(stripAnsi(line));
		if (lines.length === LINES_PER_FRAME) {
			yield lines;
			lines.length = 0;
		}
	}
}

async function main() {
	// "towel.blinkenlights.nl" no longer works on IPv4, so we'll
	// connect to "telehack.com" and type the "starwars" command
	const socket = Switch.connect('telehack.com:23');
	const reader = socket.readable.getReader();

	// wait for prompt
	while (true) {
		const chunk = await reader.read();
		if (!chunk.value) throw new Error('Connection closed');
		const value = decoder.decode(chunk.value);
		if (value.includes('Press control-C')) {
			break;
		}
	}

	// write "starwars" command
	const writer = socket.writable.getWriter();
	await writer.write(encoder.encode('starwars\r\n'));

	const ctx = Switch.screen.getContext('2d');
	const fontSize = 30.83;
	const yOffset = 180;
	ctx.font = `${fontSize}px "Geist Mono"`;

	// dump the movie to the console
	for await (const frame of frameIterator(reader)) {
		ctx.fillStyle = 'black';
		ctx.fillRect(0, 0, Switch.screen.width, Switch.screen.height);
		ctx.fillStyle = 'white';
		for (let line = 0; line < frame.length; line++) {
			const y = line * fontSize + yOffset;
			ctx.fillText(frame[line], 0, y);
		}
	}

	console.log('TCP socket closed');
}

main().catch(console.error);
