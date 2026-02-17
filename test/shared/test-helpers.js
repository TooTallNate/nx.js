/**
 * Shared test helper functions for nx.js conformance tests.
 *
 * Loaded AFTER runtime.js but BEFORE test fixtures. Provides convenience
 * functions that the fixtures use (textEncode, textDecode, toBase64,
 * fromBase64, compress, decompress).
 *
 * These are thin wrappers around the standard Web APIs that the runtime
 * provides (TextEncoder, TextDecoder, CompressionStream, DecompressionStream).
 */

// ---- Text encoding/decoding ----

function textEncode(str) {
	return new TextEncoder().encode(str);
}

function textDecode(buf) {
	return new TextDecoder().decode(buf);
}

// ---- Base64 ----

var BASE64_CHARS =
	'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';

function toBase64(bytes) {
	if (!(bytes instanceof Uint8Array)) {
		bytes = new Uint8Array(bytes);
	}
	var result = '';
	var i;
	for (i = 0; i < bytes.length - 2; i += 3) {
		var b0 = bytes[i],
			b1 = bytes[i + 1],
			b2 = bytes[i + 2];
		result += BASE64_CHARS[(b0 >> 2) & 0x3f];
		result += BASE64_CHARS[((b0 << 4) | (b1 >> 4)) & 0x3f];
		result += BASE64_CHARS[((b1 << 2) | (b2 >> 6)) & 0x3f];
		result += BASE64_CHARS[b2 & 0x3f];
	}
	if (i < bytes.length) {
		var b0 = bytes[i];
		result += BASE64_CHARS[(b0 >> 2) & 0x3f];
		if (i + 1 < bytes.length) {
			var b1 = bytes[i + 1];
			result += BASE64_CHARS[((b0 << 4) | (b1 >> 4)) & 0x3f];
			result += BASE64_CHARS[(b1 << 2) & 0x3f];
			result += '=';
		} else {
			result += BASE64_CHARS[(b0 << 4) & 0x3f];
			result += '==';
		}
	}
	return result;
}

var BASE64_LOOKUP = new Uint8Array(128);
(function () {
	for (var i = 0; i < 128; i++) BASE64_LOOKUP[i] = 0xff;
	for (var i = 0; i < BASE64_CHARS.length; i++) {
		BASE64_LOOKUP[BASE64_CHARS.charCodeAt(i)] = i;
	}
})();

function fromBase64(str) {
	// Remove padding
	var padding = 0;
	if (str.length > 0 && str[str.length - 1] === '=') padding++;
	if (str.length > 1 && str[str.length - 2] === '=') padding++;

	var outLen = (str.length * 3) / 4 - padding;
	var result = new Uint8Array(outLen);
	var j = 0;
	for (var i = 0; i < str.length; i += 4) {
		var a = BASE64_LOOKUP[str.charCodeAt(i)];
		var b = BASE64_LOOKUP[str.charCodeAt(i + 1)];
		var c = BASE64_LOOKUP[str.charCodeAt(i + 2)];
		var d = BASE64_LOOKUP[str.charCodeAt(i + 3)];
		if (j < outLen) result[j++] = (a << 2) | (b >> 4);
		if (j < outLen) result[j++] = ((b << 4) & 0xf0) | (c >> 2);
		if (j < outLen) result[j++] = ((c << 6) & 0xc0) | d;
	}
	return result;
}

// ---- Compression/Decompression ----

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

async function compress(format, data) {
	var cs = new CompressionStream(format);
	var writer = cs.writable.getWriter();
	var reader = cs.readable.getReader();
	writer.write(data);
	writer.close();
	var chunks = [];
	while (true) {
		var r = await reader.read();
		if (r.done) break;
		chunks.push(r.value);
	}
	return concatUint8Arrays(chunks);
}

async function decompress(format, data) {
	var ds = new DecompressionStream(format);
	var writer = ds.writable.getWriter();
	var reader = ds.readable.getReader();
	writer.write(data);
	writer.close();
	var chunks = [];
	while (true) {
		var r = await reader.read();
		if (r.done) break;
		chunks.push(r.value);
	}
	return concatUint8Arrays(chunks);
}
