export function singleton<K extends object, V>(
	map: WeakMap<K, V>,
	key: K,
	init: () => V,
) {
	let value = map.get(key);
	if (!value) {
		value = init();
		map.set(key, value);
	}
	return value;
}

const views = new WeakMap<ArrayBufferView, DataView>();
export function view(view: ArrayBufferView): DataView {
	return singleton(
		views,
		view,
		() => new DataView(view.buffer, view.byteOffset, view.byteLength),
	);
}

const u8s = new WeakMap<ArrayBufferView, Uint8Array>();
export function u8(view: ArrayBufferView): Uint8Array {
	return singleton(
		u8s,
		view,
		() => new Uint8Array(view.buffer, view.byteOffset, view.byteLength),
	);
}

export function decodeCString(view: ArrayBufferView): string {
	const arr = u8(view);
	const nul = arr.indexOf(0);
	return new TextDecoder().decode(nul !== -1 ? arr.subarray(0, nul) : arr);
}

export class ArrayBufferStruct implements ArrayBufferView {
	readonly buffer: ArrayBufferLike;
	readonly byteLength: number;
	readonly byteOffset: number;

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
		this.byteOffset = 0;
		if (buffer) {
			if ('buffer' in buffer) {
				this.buffer = buffer.buffer;
				this.byteOffset = buffer.byteOffset;
			} else {
				this.buffer = buffer;
			}
		} else {
			this.buffer = new ArrayBuffer(new.target.sizeof);
		}
		if (typeof byteOffset === 'number') {
			this.byteOffset += byteOffset;
		}
		this.byteLength = byteLength ?? new.target.sizeof;
	}
}
