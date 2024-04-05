import { def } from '../utils';

export interface TextEncoderEncodeIntoResult {
	read: number;
	written: number;
}

/**
 * TextEncoder takes a UTF-8 encoded string of code points as input and return a stream of bytes.
 *
 * @link https://developer.mozilla.org/docs/Web/API/TextEncoder
 */
export class TextEncoder implements globalThis.TextEncoder {
	encoding: string;

	constructor() {
		this.encoding = 'utf-8';
	}

	/**
	 * Returns the result of running UTF-8's encoder.
	 *
	 * @link https://developer.mozilla.org/docs/Web/API/TextEncoder/encode
	 */
	encode(input?: string): Uint8Array {
		if (!input) return new Uint8Array(0);
		var pos = 0;
		var len = input.length;

		var at = 0; // output position
		var tlen = Math.max(32, len + (len >>> 1) + 7); // 1.5x size
		var target = new Uint8Array((tlen >>> 3) << 3); // ... but at 8 byte offset

		while (pos < len) {
			var value = input.charCodeAt(pos++);
			if (value >= 0xd800 && value <= 0xdbff) {
				// high surrogate
				if (pos < len) {
					var extra = input.charCodeAt(pos);
					if ((extra & 0xfc00) === 0xdc00) {
						++pos;
						value = ((value & 0x3ff) << 10) + (extra & 0x3ff) + 0x10000;
					} else {
						// Lone high surrogate, encode as replacement character
						value = 0xfffd;
					}
				} else {
					// Lone high surrogate at the end of the string, encode as replacement character
					value = 0xfffd;
				}
			} else if (value >= 0xdc00 && value <= 0xdfff) {
				// Lone low surrogate, encode as replacement character
				value = 0xfffd;
			}

			if ((value & 0xffffff80) === 0) {
				// 1-byte
				target[at++] = value; // ASCII
				continue;
			} else if ((value & 0xfffff800) === 0) {
				// 2-byte
				target[at++] = ((value >>> 6) & 0x1f) | 0xc0;
			} else if ((value & 0xffff0000) === 0) {
				// 3-byte
				target[at++] = ((value >>> 12) & 0x0f) | 0xe0;
				target[at++] = ((value >>> 6) & 0x3f) | 0x80;
			} else if ((value & 0xffe00000) === 0) {
				// 4-byte
				target[at++] = ((value >>> 18) & 0x07) | 0xf0;
				target[at++] = ((value >>> 12) & 0x3f) | 0x80;
				target[at++] = ((value >>> 6) & 0x3f) | 0x80;
			} else {
				continue; // out of range
			}

			target[at++] = (value & 0x3f) | 0x80;
		}

		return target.slice(0, at);
	}

	/**
	 * Runs the UTF-8 encoder on source, stores the result of that operation into destination, and returns the progress made as an object wherein read is the number of converted code units of source and written is the number of bytes modified in destination.
	 *
	 * **Note:** Not currently supported.
	 *
	 * @link https://developer.mozilla.org/docs/Web/API/TextEncoder/encodeInto
	 */
	encodeInto(
		input: string,
		destination: Uint8Array,
	): TextEncoderEncodeIntoResult {
		let read = 0;
		let written = 0;
		const sourceLength = input.length;
		let destinationLength = destination.length;

		for (; read < sourceLength && written < destinationLength; read++) {
			const codePoint = input.charCodeAt(read);

			// Handle surrogate pairs
			if (
				codePoint >= 0xd800 &&
				codePoint <= 0xdbff &&
				read + 1 < sourceLength
			) {
				const nextCodePoint = input.charCodeAt(read + 1);
				if (nextCodePoint >= 0xdc00 && nextCodePoint <= 0xdfff) {
					// Combine the surrogate pair into a single code point
					const combinedCodePoint =
						((codePoint - 0xd800) << 10) + (nextCodePoint - 0xdc00) + 0x10000;
					if (written + 4 <= destinationLength) {
						destination[written++] = ((combinedCodePoint >> 18) & 0x07) | 0xf0;
						destination[written++] = ((combinedCodePoint >> 12) & 0x3f) | 0x80;
						destination[written++] = ((combinedCodePoint >> 6) & 0x3f) | 0x80;
						destination[written++] = (combinedCodePoint & 0x3f) | 0x80;
						read++; // Skip the next code unit as it's part of the surrogate pair
					} else {
						break; // Not enough space
					}
				} else {
					// Lone high surrogate, treat as an error and encode as replacement character
					if (written + 3 <= destinationLength) {
						destination[written++] = 0xef;
						destination[written++] = 0xbf;
						destination[written++] = 0xbd; // Replacement character
					} else {
						break; // Not enough space
					}
				}
			} else if (codePoint < 0x80) {
				// 1-byte character
				destination[written++] = codePoint;
			} else if (codePoint < 0x800) {
				// 2-byte character
				if (written + 2 <= destinationLength) {
					destination[written++] = (codePoint >> 6) | 0xc0;
					destination[written++] = (codePoint & 0x3f) | 0x80;
				} else {
					break; // Not enough space
				}
			} else if (codePoint < 0x10000) {
				// 3-byte character
				if (written + 3 <= destinationLength) {
					destination[written++] = (codePoint >> 12) | 0xe0;
					destination[written++] = ((codePoint >> 6) & 0x3f) | 0x80;
					destination[written++] = (codePoint & 0x3f) | 0x80;
				} else {
					break; // Not enough space
				}
			} else {
				// Code point out of range
				break;
			}
		}

		return { read, written };
	}
}
def(TextEncoder);

export const encoder = new TextEncoder();
