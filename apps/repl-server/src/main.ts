import { cursor } from 'sisteransi';
import { REPL } from '@nx.js/repl';

// Telnet option codes
const IAC = 255;
const WILL = 251;
const DO = 253;
const ECHO = 1;
const SUPPRESS_GO_AHEAD = 3;
const SB = 250; // Subnegotiation Begin
const SE = 240; // Subnegotiation End

/**
 * Create a TCP REPL server bound to "0.0.0.0:2323".
 */
const port = 2323;

async function* telnetParser(reader: ReadableStreamDefaultReader<Uint8Array>) {
	let buffer = new Uint8Array(0);
	while (true) {
		let { done, value } = await reader.read();
		if (done || !value) {
			break;
		}
		buffer = Uint8Array.from([...buffer, ...value]);
		while (buffer.length > 0) {
			if (buffer[0] === IAC) {
				// Find the length of the Telnet command sequence
				let cmdLength = 3; // Default for simple commands
				if (buffer[1] === SB) {
					// Find SE for subnegotiation commands
					const seIndex = buffer.indexOf(SE);
					if (seIndex === -1) {
						// SE not found, wait for more data
						break;
					}
					cmdLength = seIndex + 1;
				}
				// Yield the command sequence
				yield { type: 'command', data: buffer.slice(0, cmdLength) };
				buffer = buffer.slice(cmdLength);
			} else {
				// Find next IAC or end of buffer
				const iacIndex = buffer.indexOf(IAC);
				const rawData = iacIndex === -1 ? buffer : buffer.slice(0, iacIndex);
				if (rawData.length > 0) {
					yield { type: 'data', data: rawData };
				}
				buffer = iacIndex === -1 ? new Uint8Array() : buffer.slice(iacIndex);
			}
		}
	}
}

Switch.listen({
	port,

	// `accept` is invoked when a new TCP client has connected
	async accept({ socket }) {
		console.log('Client connection established');
		const reader = socket.readable.getReader();
		const writer = socket.writable.getWriter();
		const repl = new REPL(writer);
		try {
			// Put client in "raw" TTY mode
			await writer.write(Uint8Array.from([IAC, DO, SUPPRESS_GO_AHEAD]));
			await writer.write(Uint8Array.from([IAC, WILL, SUPPRESS_GO_AHEAD]));
			await writer.write(Uint8Array.from([IAC, WILL, ECHO]));
			await writer.write(new TextEncoder().encode(cursor.hide));
			repl.renderPrompt();

			for await (const next of telnetParser(reader)) {
				console.log(next);
				if (next.type !== 'data') {
					continue;
				}

				if (next.data[0] === 13 /* \r */) {
					await repl.submit();
				} else if (
					next.data[0] === 27 &&
					next.data[1] === 91 &&
					next.data[2] === 65
				) {
					await repl.arrowUp();
				} else if (
					next.data[0] === 27 &&
					next.data[1] === 91 &&
					next.data[2] === 66
				) {
					await repl.arrowDown();
				} else if (
					next.data[0] === 27 &&
					next.data[1] === 91 &&
					next.data[2] === 67
				) {
					await repl.arrowRight();
				} else if (
					next.data[0] === 27 &&
					next.data[1] === 91 &&
					next.data[2] === 68
				) {
					await repl.arrowLeft();
				} else if (next.data[0] === 127) {
					await repl.backspace();
				} else if (next.data[0] === 27) {
					await writer.write(new TextEncoder().encode(cursor.show));
					repl.escape();
				} else if (next.data[0] === 3 /* Ctrl+C */) {
					await writer.write(new TextEncoder().encode(cursor.show));
					socket.close();
				} else {
					await repl.write(next.data);
				}
			}
			console.log('Client closed connection');
		} catch (err) {
			console.error(err);
		}
	},
});

const { ip } = Switch.networkInfo();
console.log('REPL server listening on "%s:%d"', ip, port);
