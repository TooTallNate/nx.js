export function createSingleton<K extends object, V>(init: (k: K) => V) {
	const map = new WeakMap<K, V>();
	return (k: K) => {
		if (!map.has(k)) {
			const v = init(k);
			map.set(k, v);
			return v;
		}
		return map.get(k)!;
	};
}

export const view = createSingleton(
	(view: ArrayBufferView) =>
		new DataView(view.buffer, view.byteOffset, view.byteLength),
);

export const u8 = createSingleton(
	(view: ArrayBufferView) =>
		new Uint8Array(view.buffer, view.byteOffset, view.byteLength),
);

export function decodeCString(view: ArrayBufferView): string {
	const arr = u8(view);
	const nul = arr.indexOf(0);
	return new TextDecoder().decode(nul !== -1 ? arr.subarray(0, nul) : arr);
}

export class ArrayBufferStruct implements ArrayBufferView {
	readonly buffer!: ArrayBufferLike;
	readonly byteLength!: number;
	readonly byteOffset!: number;

	static sizeof = 0;

	constructor();
	constructor(
		buffer: ArrayBufferLike | ArrayBufferView,
		byteOffset?: number,
		byteLength?: number,
	);
	constructor(
		buffer?: ArrayBufferLike | ArrayBufferView,
		byteOffset?: number,
		byteLength?: number,
	) {
		let buf: ArrayBufferLike | undefined;
		let bo = 0;
		if (buffer) {
			if ('buffer' in buffer) {
				buf = buffer.buffer;
				bo = buffer.byteOffset;
			} else {
				buf = buffer;
			}
		} else {
			buf = new ArrayBuffer(new.target.sizeof);
		}
		if (typeof byteOffset === 'number') {
			bo += byteOffset;
		}
		Object.defineProperties(this, {
			buffer: { value: buf, enumerable: false },
			byteLength: { value: byteLength ?? new.target.sizeof, enumerable: false },
			byteOffset: { value: bo, enumerable: false },
		});
	}
}
