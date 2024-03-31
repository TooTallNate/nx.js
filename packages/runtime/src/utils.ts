import type { PathLike } from './switch';
import type { BufferSource } from './types';
import {
	INTERNAL_SYMBOL,
	type Callback,
	type CallbackArguments,
	type CallbackReturnType,
	type RGBA,
} from './internal';

export const proto = <T extends new (...args: any) => any>(
	o: any,
	c: T,
): InstanceType<T> => {
	Object.setPrototypeOf(o, c.prototype);
	return o;
};

export const def = <T extends any>(value: T, key?: string) => {
	if (!key) {
		key = (value as any).name;
		if (!key) {
			throw new Error(`Name not specified`);
		}
	}
	const proto = (value as any).prototype;
	const isClass = key[0] === key[0].toUpperCase();
	if (isClass) {
		Object.defineProperty(proto || value, Symbol.toStringTag, {
			value: key,
		});
	}
	Object.defineProperty(globalThis, key, {
		value,
		writable: true,
		enumerable: !isClass,
		configurable: true,
	});
	return value;
};

export function bufferSourceToArrayBuffer(input: BufferSource): ArrayBuffer {
	return input instanceof ArrayBuffer
		? input
		: input.buffer.slice(input.byteOffset, input.byteLength);
}

export function asyncIteratorToStream<T>(it: AsyncIterableIterator<T>) {
	return new ReadableStream<T>({
		async pull(controller) {
			const next = await it.next();
			if (next.done) {
				controller.close();
			} else {
				controller.enqueue(next.value);
			}
		},
	});
}

export function toPromise<
	Func extends (cb: Callback<any>, ...args: any[]) => any,
>(fn: Func, ...args: CallbackArguments<Func>) {
	return new Promise<CallbackReturnType<Func>>((resolve, reject) => {
		try {
			fn(
				(err, result) => {
					if (err) return reject(err);
					resolve(result);
				},
				...args,
			);
		} catch (err) {
			reject(err);
		}
	});
}

export function assertInternalConstructor(a: ArrayLike<any>) {
	if (a[0] !== INTERNAL_SYMBOL) throw new TypeError('Illegal constructor');
}

export function pathToString(p: PathLike): string {
	return typeof p === 'string' ? p : decodeURI(p.href);
}

export class Deferred<T> {
	pending = true;
	promise: Promise<T>;
	resolve!: (value: T | PromiseLike<T>) => void;
	reject!: (v: any) => void;
	constructor() {
		this.promise = new Promise<T>((res, rej) => {
			this.resolve = (v) => {
				this.pending = false;
				res(v);
			};
			this.reject = (v) => {
				this.pending = false;
				rej(v);
			};
		});
	}
}

export const createInternal = <K extends object, V>() => {
	const wm = new WeakMap<K, V>();
	const _ = (k: K): V => {
		const v = wm.get(k);
		if (!v)
			throw new Error(`Failed to get \`${k.constructor.name}\` internal state`);
		return v;
	};
	_.set = (k: K, v: V) => {
		wm.set(k, v);
	};
	return _;
};

export function rgbaToString(rgba: RGBA) {
	if (rgba[3] < 1) {
		return `rgba(${rgba.join(', ')})`;
	}
	return `#${rgba
		.slice(0, -1)
		.map((v) => v.toString(16).padStart(2, '0'))
		.join('')}`;
}

export function returnOnThrow<T extends (...args: any[]) => any>(
	fn: T,
	...args: Parameters<T>
): ReturnType<T> | Error {
	try {
		return fn(...args);
	} catch (err: any) {
		return err;
	}
}

export function stub(): never {
	throw new Error(
		'This is a stub function which should have been replaced by a native implementation',
	);
}
