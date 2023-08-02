import { def } from './utils';
import { type Switch as _Switch } from './switch';

declare const Switch: _Switch;

class Crypto implements globalThis.Crypto {
	get subtle(): SubtleCrypto {
        throw new Error('`crypto.subtle` is not yet implemented');
    }

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
        let i = 0;
        const bytes = new Uint8Array(31);
        this.getRandomValues(bytes);
		return '10000000-1000-4000-8000-100000000000'.replace(/[018]/g, (v) => {
			const c = +v;
			return (c ^ (bytes[i++] & (15 >> (c / 4)))).toString(16);
		}) as `${string}-${string}-${string}-${string}-${string}`;
	}
}
def('Crypto', Crypto);
def('crypto', new Crypto());
