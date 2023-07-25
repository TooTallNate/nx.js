export const def = (key: string, value: unknown) =>
	Object.defineProperty(globalThis, key, {
		value,
		writable: true,
		enumerable: false,
		configurable: true,
	});

export function bufferSourceToArrayBuffer(input: BufferSource): ArrayBuffer {
	return input instanceof ArrayBuffer ? input : input.buffer;
}
