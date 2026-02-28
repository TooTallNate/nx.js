import { $ } from './$';

// Registers toBase64(), toHex(), setFromBase64(), setFromHex() on
// Uint8Array.prototype and fromBase64(), fromHex() on Uint8Array — all in C.
$.uint8arrayInit(Uint8Array);

export interface Uint8ArrayToBase64Options {
	alphabet?: 'base64' | 'base64url';
	omitPadding?: boolean;
}

export interface Uint8ArrayFromBase64Options {
	alphabet?: 'base64' | 'base64url';
	lastChunkHandling?: 'loose' | 'strict' | 'stop-before-partial';
}

export interface Uint8ArraySetFromResult {
	read: number;
	written: number;
}

declare global {
	interface Uint8Array {
		/**
		 * Returns a base64-encoded string of this `Uint8Array`'s data.
		 *
		 * @param options An optional object with `alphabet` (`"base64"` or `"base64url"`) and `omitPadding` (boolean) properties.
		 * @see https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Uint8Array/toBase64
		 */
		toBase64(options?: Uint8ArrayToBase64Options): string;

		/**
		 * Returns a hex-encoded string of this `Uint8Array`'s data.
		 *
		 * @see https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Uint8Array/toHex
		 */
		toHex(): string;

		/**
		 * Populates this `Uint8Array` with bytes from a base64-encoded string.
		 *
		 * @param string A base64-encoded string.
		 * @param options An optional object with `alphabet` and `lastChunkHandling` properties.
		 * @returns An object with `read` (characters consumed) and `written` (bytes written) properties.
		 * @see https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Uint8Array/setFromBase64
		 */
		setFromBase64(
			string: string,
			options?: Uint8ArrayFromBase64Options,
		): Uint8ArraySetFromResult;

		/**
		 * Populates this `Uint8Array` with bytes from a hex-encoded string.
		 *
		 * @param string A hex-encoded string.
		 * @returns An object with `read` (characters consumed) and `written` (bytes written) properties.
		 * @see https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Uint8Array/setFromHex
		 */
		setFromHex(string: string): Uint8ArraySetFromResult;
	}

	interface Uint8ArrayConstructor {
		/**
		 * Creates a new `Uint8Array` from a base64-encoded string.
		 *
		 * @param base64 A base64-encoded string.
		 * @param options An optional object with `alphabet` and `lastChunkHandling` properties.
		 * @see https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Uint8Array/fromBase64
		 */
		fromBase64(
			base64: string,
			options?: Uint8ArrayFromBase64Options,
		): Uint8Array;

		/**
		 * Creates a new `Uint8Array` from a hex-encoded string.
		 *
		 * @param hex A hex-encoded string.
		 * @see https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Uint8Array/fromHex
		 */
		fromHex(hex: string): Uint8Array;
	}
}
