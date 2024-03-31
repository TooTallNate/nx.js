import { concat, indexOfEol, decoder } from './util';
import { UnshiftableStream } from './unshiftable-readable-stream';

export async function readHeaders(
	unshiftable: UnshiftableStream,
): Promise<string[]> {
	let leftover: Uint8Array | null = null;
	const reader = unshiftable.readable.getReader();
	const lines: string[] = [];
	while (true) {
		unshiftable.resume();
		const next = await reader.read();
		unshiftable.pause();
		if (next.done) return lines;
		const chunk: Uint8Array = leftover
			? concat(leftover, next.value)
			: next.value;
		let pos = 0;
		while (true) {
			const eol = indexOfEol(chunk, pos);
			if (eol === -1) {
				leftover = chunk.slice(pos);
				break;
			}
			const line = decoder.decode(chunk.slice(pos, eol));
			pos = eol + 2;
			if (line) {
				lines.push(line);
			} else {
				// end of headers
				unshiftable.unshift(chunk.slice(pos));
				reader.releaseLock();
				return lines;
			}
		}
	}
}

export function toHeaders(input: string[]) {
	const headers = new Headers();
	for (const line of input) {
		const col = line.indexOf(':');
		const name = line.slice(0, col);
		const value = line.slice(col + 1).trim();
		headers.set(name, value);
	}
	return headers;
}
