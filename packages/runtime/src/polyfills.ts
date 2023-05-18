export const def = (key: string, value: unknown) =>
	Object.defineProperty(globalThis, key, {
		value,
		writable: true,
		enumerable: false,
		configurable: true,
	});

import 'core-js/actual/url';
import 'core-js/actual/url-search-params';
import EventTarget from '@ungap/event-target';
def('EventTarget', EventTarget);

class TextDecoder implements globalThis.TextDecoder {
	encoding: string;
	fatal: boolean;
	ignoreBOM: boolean;
	constructor(label?: string) {
		if (typeof label === 'string' && label !== 'utf-8') {
			throw new TypeError('Only "utf-8" decoding is supported');
		}
		this.encoding = 'utf-8';
		this.fatal = false;
		this.ignoreBOM = false;
	}
	decode(input?: BufferSource | undefined): string {
		if (!input) return '';
		const buf = input instanceof ArrayBuffer ? input : input.buffer;
		return utf8ArrayToStr(new Uint8Array(buf));
	}
}
def('TextDecoder', TextDecoder);

var utf8ArrayToStr = (function () {
	const charCache = new Array(128); // Preallocate the cache for the common single byte chars
	const charFromCodePt = String.fromCodePoint || String.fromCharCode;
	const result: string[] = [];

	return function (array: ArrayLike<number>) {
		var codePt, byte1;
		var buffLen = array.length;

		result.length = 0;

		for (var i = 0; i < buffLen; ) {
			byte1 = array[i++];

			if (byte1 <= 0x7f) {
				codePt = byte1;
			} else if (byte1 <= 0xdf) {
				codePt = ((byte1 & 0x1f) << 6) | (array[i++] & 0x3f);
			} else if (byte1 <= 0xef) {
				codePt =
					((byte1 & 0x0f) << 12) |
					((array[i++] & 0x3f) << 6) |
					(array[i++] & 0x3f);
			} else {
				codePt =
					((byte1 & 0x07) << 18) |
					((array[i++] & 0x3f) << 12) |
					((array[i++] & 0x3f) << 6) |
					(array[i++] & 0x3f);
			}

			result.push(
				charCache[codePt] ||
					(charCache[codePt] = charFromCodePt(codePt))
			);
		}

		return result.join('');
	};
})();
