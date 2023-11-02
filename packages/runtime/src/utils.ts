import type { BufferSource } from './types';
import {
	INTERNAL_SYMBOL,
	type Callback,
	type CallbackArguments,
	type CallbackReturnType,
} from './internal';

export const def = <T>(key: string, value: T) => {
	const proto = (value as any).prototype;
	if (typeof proto === 'object') {
		Object.defineProperty(proto, Symbol.toStringTag, {
			value: key,
		});
	}
	Object.defineProperty(globalThis, key, {
		value,
		writable: true,
		enumerable: false,
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
	Func extends (cb: Callback<any>, ...args: any[]) => any
>(fn: Func, ...args: CallbackArguments<Func>) {
	return new Promise<CallbackReturnType<Func>>((resolve, reject) => {
		fn((err, result) => {
			if (err) return reject(err);
			resolve(result);
		}, ...args);
	});
}

export function assertInternalConstructor(a: ArrayLike<any>) {
	if (a[0] !== INTERNAL_SYMBOL) throw new TypeError('Illegal constructor');
}

export class Deferred<T> {
	pending = true;
	promise: Promise<T>;
	resolve!: (value: T | PromiseLike<T>) => void;
	reject!: (v: any) => void;
	constructor() {
		this.promise = new Promise<T>((res, rej) => {
			this.resolve = res;
			this.reject = rej;
		});
	}
}
