import { assertInternalConstructor, def } from './utils';
import { type SwitchClass } from './switch';
import { type ArrayBufferView } from './types';
import { INTERNAL_SYMBOL } from './internal';

declare const Switch: SwitchClass;

/**
 * Basic cryptography features available in the current context.
 * It allows access to a cryptographically strong random number
 * generator and to cryptographic primitives.
 */
export class Crypto implements globalThis.Crypto {
	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
	}

	/**
	 * The `crypto.subtle` interface is not yet implemented.
	 * An error will be thrown if this property is accessed.
	 */
	get subtle(): SubtleCrypto {
		throw new Error('`crypto.subtle` is not yet implemented');
	}

	/**
	 * Fills the provided `TypedArray` with cryptographically strong random values.
	 *
	 * @example
	 *
	 * ```typescript
	 * const array = new Uint32Array(10);
	 * crypto.getRandomValues(array);
	 *
	 * console.log("Your lucky numbers:");
	 * for (const num of array) {
	 *   console.log(num);
	 * }
	 * ```
	 *
	 * @param array The `TypedArray` to fill with random values.
	 * @returns The same `TypedArray` filled with random values.
	 */
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

	/**
	 * Generates a cryptographically strong random unique identifier (UUID).
	 *
	 * @example
	 *
	 * ```typescript
	 * const uuid = crypto.randomUUID();
	 * console.log(uuid);
	 * // for example "36b8f84d-df4e-4d49-b662-bcde71a8764f"
	 * ```
	 *
	 * @returns A string representation of a UUID.
	 */
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

/**
 * The global `crypto` property returns the {@link Crypto} object associated to the global object.
 * This object allows your application to access to certain cryptographic related services.
 *
 * @see https://developer.mozilla.org/docs/Web/API/crypto_property
 */
// @ts-expect-error Internal constructor
export const crypto = new Crypto(INTERNAL_SYMBOL);
def('crypto', crypto);
