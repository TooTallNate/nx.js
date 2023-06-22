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
import { Event, UIEvent, KeyboardEvent, TouchEvent } from './event';
def('EventTarget', EventTarget);
def('Event', Event);
def('UIEvent', UIEvent);
def('KeyboardEvent', KeyboardEvent);
def('TouchEvent', TouchEvent);

/**
 * Credit for `TextEncoder` and `TextDecoder` goes to Sam Thorogood.
 * Apache License 2.0
 * https://github.com/samthor/fast-text-encoding/blob/master/src/lowlevel.js
 */
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
		let bytes = new Uint8Array(buf);
		var inputIndex = 0;

		// Create a working buffer for UTF-16 code points, but don't generate one
		// which is too large for small input sizes. UTF-8 to UCS-16 conversion is
		// going to be at most 1:1, if all code points are ASCII. The other extreme
		// is 4-byte UTF-8, which results in two UCS-16 points, but this is still 50%
		// fewer entries in the output.
		var pendingSize = Math.min(256 * 256, bytes.length + 1);
		var pending = new Uint16Array(pendingSize);
		var chunks = [];
		var pendingIndex = 0;

		for (;;) {
			var more = inputIndex < bytes.length;

			// If there's no more data or there'd be no room for two UTF-16 values,
			// create a chunk. This isn't done at the end by simply slicing the data
			// into equal sized chunks as we might hit a surrogate pair.
			if (!more || pendingIndex >= pendingSize - 1) {
				// nb. .apply and friends are *really slow*. Low-hanging fruit is to
				// expand this to literally pass pending[0], pending[1], ... etc, but
				// the output code expands pretty fast in this case.
				// These extra vars get compiled out: they're just to make TS happy.
				// Turns out you can pass an ArrayLike to .apply().
				var subarray = pending.subarray(0, pendingIndex);
				// @ts-expect-error
				chunks.push(String.fromCharCode.apply(null, subarray));

				if (!more) {
					return chunks.join('');
				}

				// Move the buffer forward and create another chunk.
				bytes = bytes.subarray(inputIndex);
				inputIndex = 0;
				pendingIndex = 0;
			}

			// The native TextDecoder will generate "REPLACEMENT CHARACTER" where the
			// input data is invalid. Here, we blindly parse the data even if it's
			// wrong: e.g., if a 3-byte sequence doesn't have two valid continuations.

			var byte1 = bytes[inputIndex++];
			if ((byte1 & 0x80) === 0) {
				// 1-byte or null
				pending[pendingIndex++] = byte1;
			} else if ((byte1 & 0xe0) === 0xc0) {
				// 2-byte
				var byte2 = bytes[inputIndex++] & 0x3f;
				pending[pendingIndex++] = ((byte1 & 0x1f) << 6) | byte2;
			} else if ((byte1 & 0xf0) === 0xe0) {
				// 3-byte
				var byte2 = bytes[inputIndex++] & 0x3f;
				var byte3 = bytes[inputIndex++] & 0x3f;
				pending[pendingIndex++] =
					((byte1 & 0x1f) << 12) | (byte2 << 6) | byte3;
			} else if ((byte1 & 0xf8) === 0xf0) {
				// 4-byte
				var byte2 = bytes[inputIndex++] & 0x3f;
				var byte3 = bytes[inputIndex++] & 0x3f;
				var byte4 = bytes[inputIndex++] & 0x3f;

				// this can be > 0xffff, so possibly generate surrogates
				var codepoint =
					((byte1 & 0x07) << 0x12) |
					(byte2 << 0x0c) |
					(byte3 << 0x06) |
					byte4;
				if (codepoint > 0xffff) {
					// codepoint &= ~0x10000;
					codepoint -= 0x10000;
					pending[pendingIndex++] =
						((codepoint >>> 10) & 0x3ff) | 0xd800;
					codepoint = 0xdc00 | (codepoint & 0x3ff);
				}
				pending[pendingIndex++] = codepoint;
			} else {
				// invalid initial byte
				pending[pendingIndex++] = 0xfffd;
			}
		}
	}
}
def('TextDecoder', TextDecoder);

class TextEncoder implements globalThis.TextEncoder {
	encoding: string;
	constructor() {
		this.encoding = 'utf-8';
	}
	encode(input?: string | undefined): Uint8Array {
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
						value =
							((value & 0x3ff) << 10) + (extra & 0x3ff) + 0x10000;
					}
				}
				if (value >= 0xd800 && value <= 0xdbff) {
					continue; // drop lone surrogate
				}
			}

			// expand the buffer if we couldn't write 4 bytes
			if (at + 4 > target.length) {
				tlen += 8; // minimum extra
				tlen *= 1.0 + (pos / input.length) * 2; // take 2x the remaining
				tlen = (tlen >>> 3) << 3; // 8 byte offset

				var update = new Uint8Array(tlen);
				update.set(target);
				target = update;
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
	encodeInto(
		source: string,
		destination: Uint8Array
	): TextEncoderEncodeIntoResult {
		throw new Error('Method not implemented.');
	}
}
def('TextEncoder', TextEncoder);
