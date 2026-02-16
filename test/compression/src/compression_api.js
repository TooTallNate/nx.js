/**
 * Compression Streams API Bridge for nx.js test harness
 *
 * This script runs inside QuickJS and bridges the compression/decompression
 * API to the native nx.js C bindings exposed via the '$' global (init_obj).
 *
 * Provides:
 *   compress(format, data)   -> Promise<Uint8Array>
 *   decompress(format, data) -> Promise<Uint8Array>
 *   toBase64(uint8array)     -> string
 *   fromBase64(string)       -> Uint8Array
 *   textEncode(string)       -> Uint8Array
 *   textDecode(uint8array)   -> string
 */

// ---- Base64 encoder/decoder ----
// QuickJS doesn't have btoa/atob, so we implement our own.

var BASE64_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

function toBase64(bytes) {
	if (!(bytes instanceof Uint8Array)) {
		bytes = new Uint8Array(bytes);
	}
	var result = "";
	var i;
	for (i = 0; i < bytes.length - 2; i += 3) {
		var b0 = bytes[i], b1 = bytes[i + 1], b2 = bytes[i + 2];
		result += BASE64_CHARS[(b0 >> 2) & 0x3F];
		result += BASE64_CHARS[((b0 << 4) | (b1 >> 4)) & 0x3F];
		result += BASE64_CHARS[((b1 << 2) | (b2 >> 6)) & 0x3F];
		result += BASE64_CHARS[b2 & 0x3F];
	}
	if (i < bytes.length) {
		var b0 = bytes[i];
		result += BASE64_CHARS[(b0 >> 2) & 0x3F];
		if (i + 1 < bytes.length) {
			var b1 = bytes[i + 1];
			result += BASE64_CHARS[((b0 << 4) | (b1 >> 4)) & 0x3F];
			result += BASE64_CHARS[(b1 << 2) & 0x3F];
			result += "=";
		} else {
			result += BASE64_CHARS[(b0 << 4) & 0x3F];
			result += "==";
		}
	}
	return result;
}

var BASE64_LOOKUP = new Uint8Array(128);
(function() {
	for (var i = 0; i < 128; i++) BASE64_LOOKUP[i] = 0xFF;
	for (var i = 0; i < BASE64_CHARS.length; i++) {
		BASE64_LOOKUP[BASE64_CHARS.charCodeAt(i)] = i;
	}
})();

function fromBase64(str) {
	// Remove padding
	var padding = 0;
	if (str.length > 0 && str[str.length - 1] === "=") padding++;
	if (str.length > 1 && str[str.length - 2] === "=") padding++;

	var outLen = (str.length * 3 / 4) - padding;
	var result = new Uint8Array(outLen);
	var j = 0;
	for (var i = 0; i < str.length; i += 4) {
		var a = BASE64_LOOKUP[str.charCodeAt(i)];
		var b = BASE64_LOOKUP[str.charCodeAt(i + 1)];
		var c = BASE64_LOOKUP[str.charCodeAt(i + 2)];
		var d = BASE64_LOOKUP[str.charCodeAt(i + 3)];
		if (j < outLen) result[j++] = (a << 2) | (b >> 4);
		if (j < outLen) result[j++] = ((b << 4) & 0xF0) | (c >> 2);
		if (j < outLen) result[j++] = ((c << 6) & 0xC0) | d;
	}
	return result;
}

// ---- Text encoder/decoder (UTF-8) ----

function textEncode(str) {
	// Simple UTF-8 encoder
	var bytes = [];
	for (var i = 0; i < str.length; i++) {
		var code = str.charCodeAt(i);
		if (code < 0x80) {
			bytes.push(code);
		} else if (code < 0x800) {
			bytes.push(0xC0 | (code >> 6));
			bytes.push(0x80 | (code & 0x3F));
		} else if (code >= 0xD800 && code <= 0xDBFF) {
			// Surrogate pair
			var next = str.charCodeAt(++i);
			var cp = ((code - 0xD800) << 10) + (next - 0xDC00) + 0x10000;
			bytes.push(0xF0 | (cp >> 18));
			bytes.push(0x80 | ((cp >> 12) & 0x3F));
			bytes.push(0x80 | ((cp >> 6) & 0x3F));
			bytes.push(0x80 | (cp & 0x3F));
		} else {
			bytes.push(0xE0 | (code >> 12));
			bytes.push(0x80 | ((code >> 6) & 0x3F));
			bytes.push(0x80 | (code & 0x3F));
		}
	}
	return new Uint8Array(bytes);
}

function textDecode(bytes) {
	if (!(bytes instanceof Uint8Array)) {
		bytes = new Uint8Array(bytes);
	}
	var result = "";
	var i = 0;
	while (i < bytes.length) {
		var b = bytes[i];
		if (b < 0x80) {
			result += String.fromCharCode(b);
			i++;
		} else if ((b & 0xE0) === 0xC0) {
			result += String.fromCharCode(((b & 0x1F) << 6) | (bytes[i + 1] & 0x3F));
			i += 2;
		} else if ((b & 0xF0) === 0xE0) {
			result += String.fromCharCode(
				((b & 0x0F) << 12) | ((bytes[i + 1] & 0x3F) << 6) | (bytes[i + 2] & 0x3F)
			);
			i += 3;
		} else if ((b & 0xF8) === 0xF0) {
			var cp = ((b & 0x07) << 18) | ((bytes[i + 1] & 0x3F) << 12) |
			         ((bytes[i + 2] & 0x3F) << 6) | (bytes[i + 3] & 0x3F);
			// Convert to surrogate pair
			cp -= 0x10000;
			result += String.fromCharCode(0xD800 + (cp >> 10), 0xDC00 + (cp & 0x3FF));
			i += 4;
		} else {
			i++; // skip invalid byte
		}
	}
	return result;
}

// ---- Uint8Array concatenation ----

function concatUint8Arrays(arrays) {
	var totalLength = 0;
	for (var i = 0; i < arrays.length; i++) {
		totalLength += arrays[i].length;
	}
	var result = new Uint8Array(totalLength);
	var offset = 0;
	for (var i = 0; i < arrays.length; i++) {
		result.set(arrays[i], offset);
		offset += arrays[i].length;
	}
	return result;
}

// ---- Compression/Decompression API ----

function compress(format, data) {
	var handle = $.compressNew(format);
	return $.compressWrite(handle, data).then(function(written) {
		var chunks = [];
		if (written) chunks.push(new Uint8Array(written));
		return $.compressFlush(handle).then(function(flushed) {
			if (flushed) chunks.push(new Uint8Array(flushed));
			return concatUint8Arrays(chunks);
		});
	});
}

function decompress(format, data) {
	var handle = $.decompressNew(format);
	return $.decompressWrite(handle, data).then(function(written) {
		var chunks = [];
		if (written) chunks.push(new Uint8Array(written));
		return $.decompressFlush(handle).then(function(flushed) {
			if (flushed) chunks.push(new Uint8Array(flushed));
			return concatUint8Arrays(chunks);
		});
	});
}

// Expose globals for fixtures
globalThis.compress = compress;
globalThis.decompress = decompress;
globalThis.toBase64 = toBase64;
globalThis.fromBase64 = fromBase64;
globalThis.textEncode = textEncode;
globalThis.textDecode = textDecode;
globalThis.concatUint8Arrays = concatUint8Arrays;
