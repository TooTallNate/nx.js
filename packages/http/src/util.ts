import type { UnshiftableStream } from './unshiftable-readable-stream';

export const encoder = new TextEncoder();
export const decoder = new TextDecoder();

export function indexOfEol(arr: ArrayLike<number>, offset = 0): number {
	for (let i = offset; i < arr.length - 1; i++) {
		if (arr[i] === 13 && arr[i + 1] === 10) {
			return i;
		}
	}
	return -1;
}

export function concat(a: Uint8Array, b: Uint8Array) {
	const c = new Uint8Array(a.length + b.length);
	c.set(a, 0);
	c.set(b, a.length);
	return c;
}

export async function readUntilEol(
	reader: ReadableStreamDefaultReader<Uint8Array>,
	unshiftable: UnshiftableStream,
): Promise<string> {
	let buffered = new Uint8Array(0);
	while (true) {
		unshiftable.resume();
		const { done, value } = await reader.read();
		unshiftable.pause();
		if (done) break;
		buffered = concat(buffered, value);
		const i = indexOfEol(buffered);
		if (i !== -1) {
			if (buffered.length > i + 2) {
				unshiftable.unshift(buffered.slice(i + 2));
				buffered = buffered.slice(0, i);
			}
			break;
		}
	}
	return decoder.decode(buffered);
}

export async function flushBytes(
	controller: ReadableStreamDefaultController<Uint8Array>,
	numBytes: number,
	reader: ReadableStreamDefaultReader<Uint8Array>,
	unshiftable: UnshiftableStream,
): Promise<void> {
	let numFlushed = 0;
	while (true) {
		unshiftable.resume();
		const { done, value } = await reader.read();
		unshiftable.pause();
		if (done) break;
		const numRemaining = numBytes - numFlushed;
		if (value.length > numRemaining) {
			unshiftable.unshift(value.slice(numRemaining));
			const chunk = value.slice(0, numRemaining);
			numFlushed += chunk.length;
			controller.enqueue(chunk);
			break;
		} else {
			numFlushed += value.length;
			controller.enqueue(value);
		}
	}
	if (numFlushed !== numBytes) {
		// TODO: throw error?
	}
}
