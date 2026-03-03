export function toHex(arr: ArrayBuffer): string {
	return new Uint8Array(arr).toHex();
}

export function fromHex(hex: string): ArrayBuffer {
	return Uint8Array.fromHex(hex).buffer;
}

export function concat(...buffers: ArrayBuffer[]): ArrayBuffer {
	const size = buffers.reduce((acc, buf) => acc + buf.byteLength, 0);
	let offset = 0;
	const result = new Uint8Array(size);
	for (const buf of buffers) {
		result.set(new Uint8Array(buf), offset);
		offset += buf.byteLength;
	}
	return result.buffer;
}
