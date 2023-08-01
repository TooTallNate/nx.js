export const def = (key: string, value: any) => {
	if (typeof value.prototype === 'object') {
		Object.defineProperty(value.prototype, Symbol.toStringTag, {
			value: key,
		});
	}
	Object.defineProperty(globalThis, key, {
		value,
		writable: true,
		enumerable: false,
		configurable: true,
	});
};

export function bufferSourceToArrayBuffer(input: BufferSource): ArrayBuffer {
	return input instanceof ArrayBuffer ? input : input.buffer;
}
