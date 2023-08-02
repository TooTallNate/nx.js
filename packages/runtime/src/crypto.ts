import { type Switch as _Switch } from './switch';
import { bufferSourceToArrayBuffer, def } from './utils';

declare const Switch: _Switch;

class Crypto implements globalThis.Crypto {
	subtle!: SubtleCrypto;

	getRandomValues<T extends ArrayBufferView | null>(array: T): T {
		if (array) {
			Switch.native.cryptoRandomBytes(
				array.buffer,
				array.byteOffset,
				array.byteLength
			);
		}
		return array;
	}

	randomUUID(): `${string}-${string}-${string}-${string}-${string}` {
		return '10000000-1000-4000-8000-100000000000'.replace(/[018]/g, (v) => {
			const c = +v;
			return (
				c ^
				(this.getRandomValues(new Uint8Array(1))[0] & (15 >> (c / 4)))
			).toString(16);
		}) as `${string}-${string}-${string}-${string}-${string}`;
	}
}
def('Crypto', Crypto);
def('crypto', new Crypto());
