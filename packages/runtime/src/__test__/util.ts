export function toHex(arr: ArrayBuffer): string {
	return Array.from(new Uint8Array(arr))
		.map((v) => v.toString(16).padStart(2, '0'))
		.join('');
}

export function fromHex(hex: string): ArrayBuffer {
	const arr = new Uint8Array(hex.length / 2);
	for (let i = 0; i < hex.length; i += 2) {
		arr[i / 2] = parseInt(hex.substr(i, 2), 16);
	}
	return arr.buffer;
}
