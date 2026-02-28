#include "uint8array.h"
#include <mbedtls/base64.h>
#include <stdlib.h>
#include <string.h>

static void free_array_buffer(JSRuntime *rt, void *opaque, void *ptr) {
	free(ptr);
}

static const char hex_chars[] = "0123456789abcdef";

// Stored reference to the Uint8Array constructor
static JSValue uint8array_ctor = JS_UNINITIALIZED;

// Helper: create a new Uint8Array from an ArrayBuffer, with OOM safety
static JSValue new_uint8array(JSContext *ctx, uint8_t *data, size_t len) {
	JSValue array_buf = JS_NewArrayBuffer(ctx, data, len,
										  free_array_buffer, NULL, false);
	if (JS_IsException(array_buf)) {
		free(data);
		return JS_EXCEPTION;
	}
	JSValue result = JS_CallConstructor(ctx, uint8array_ctor, 1, &array_buf);
	JS_FreeValue(ctx, array_buf);
	return result;
}

// Helper: validate that `this` is a Uint8Array instance
static int validate_uint8array(JSContext *ctx, JSValueConst this_val) {
	if (!JS_IsInstanceOf(ctx, this_val, uint8array_ctor)) {
		JS_ThrowTypeError(ctx, "Method requires a Uint8Array receiver");
		return -1;
	}
	return 0;
}

// ---- Base64 alphabet helpers ----

static int b64_char_value(char c, int url_safe) {
	if (c >= 'A' && c <= 'Z') return c - 'A';
	if (c >= 'a' && c <= 'z') return c - 'a' + 26;
	if (c >= '0' && c <= '9') return c - '0' + 52;
	if (url_safe) {
		if (c == '-') return 62;
		if (c == '_') return 63;
	} else {
		if (c == '+') return 62;
		if (c == '/') return 63;
	}
	return -1;
}

static int is_ascii_whitespace(char c) {
	return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}

// ---- Option parsing helpers ----

enum {
	LAST_CHUNK_LOOSE = 0,
	LAST_CHUNK_STRICT = 1,
	LAST_CHUNK_STOP_BEFORE_PARTIAL = 2,
};

// Validate options argument per GetOptionsObject:
// undefined → defaults, object → use it, anything else → TypeError.
// Returns 0 if options should use defaults (undefined), 1 if valid object, -1 on error.
static int validate_options(JSContext *ctx, JSValueConst options) {
	if (JS_IsUndefined(options))
		return 0;
	if (JS_IsObject(options))
		return 1;
	JS_ThrowTypeError(ctx, "Options must be an object or undefined");
	return -1;
}

// Parse the `alphabet` option. Returns 0 for base64, 1 for base64url, -1 on error.
static int parse_alphabet_option(JSContext *ctx, JSValueConst options) {
	int v = validate_options(ctx, options);
	if (v <= 0) return v; // 0 for defaults, -1 for error
	JSValue val = JS_GetPropertyStr(ctx, options, "alphabet");
	if (JS_IsUndefined(val)) {
		JS_FreeValue(ctx, val);
		return 0;
	}
	const char *str = JS_ToCString(ctx, val);
	JS_FreeValue(ctx, val);
	if (!str) return -1;
	int result;
	if (strcmp(str, "base64") == 0)
		result = 0;
	else if (strcmp(str, "base64url") == 0)
		result = 1;
	else {
		JS_FreeCString(ctx, str);
		JS_ThrowTypeError(ctx, "Invalid alphabet");
		return -1;
	}
	JS_FreeCString(ctx, str);
	return result;
}

// Parse the `omitPadding` option. Returns 0 or 1, or -1 on error.
static int parse_omit_padding_option(JSContext *ctx, JSValueConst options) {
	int v = validate_options(ctx, options);
	if (v <= 0) return v;
	JSValue val = JS_GetPropertyStr(ctx, options, "omitPadding");
	if (JS_IsUndefined(val)) {
		JS_FreeValue(ctx, val);
		return 0;
	}
	int result = JS_ToBool(ctx, val);
	JS_FreeValue(ctx, val);
	return result;
}

// Parse the `lastChunkHandling` option. Returns enum value or -1 on error.
static int parse_last_chunk_option(JSContext *ctx, JSValueConst options) {
	int v = validate_options(ctx, options);
	if (v < 0) return -1;
	if (v == 0) return LAST_CHUNK_LOOSE;
	JSValue val = JS_GetPropertyStr(ctx, options, "lastChunkHandling");
	if (JS_IsUndefined(val)) {
		JS_FreeValue(ctx, val);
		return LAST_CHUNK_LOOSE;
	}
	const char *str = JS_ToCString(ctx, val);
	JS_FreeValue(ctx, val);
	if (!str) return -1;
	int result;
	if (strcmp(str, "loose") == 0)
		result = LAST_CHUNK_LOOSE;
	else if (strcmp(str, "strict") == 0)
		result = LAST_CHUNK_STRICT;
	else if (strcmp(str, "stop-before-partial") == 0)
		result = LAST_CHUNK_STOP_BEFORE_PARTIAL;
	else {
		JS_FreeCString(ctx, str);
		JS_ThrowTypeError(ctx, "Invalid lastChunkHandling option");
		return -1;
	}
	JS_FreeCString(ctx, str);
	return result;
}

static int hex_digit(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

// ---- Custom base64 decode with full spec compliance ----
// Handles: whitespace stripping, alphabet selection, lastChunkHandling,
// partial output (for setFromBase64), and returns read/written counts.
//
// Parameters:
//   input, input_len: the base64 string
//   output: buffer to write decoded bytes (can be NULL for size calculation)
//   max_output: maximum bytes to write (SIZE_MAX for no limit)
//   url_safe: 0 for base64, 1 for base64url
//   last_chunk_handling: LAST_CHUNK_LOOSE, LAST_CHUNK_STRICT, LAST_CHUNK_STOP_BEFORE_PARTIAL
//   out_read: number of input characters consumed
//   out_written: number of output bytes written
//
// Returns 0 on success, -1 on error (throws JS exception via ctx).
static int base64_decode_impl(
	JSContext *ctx,
	const char *input, size_t input_len,
	uint8_t *output, size_t max_output,
	int url_safe, int last_chunk_handling,
	size_t *out_read, size_t *out_written
) {
	// Per spec step 10.3: if maxLength is 0, return immediately
	if (max_output == 0) {
		*out_read = 0;
		*out_written = 0;
		return 0;
	}

	size_t written = 0;
	size_t read = 0;  // tracks position after last fully consumed chunk
	size_t i = 0;
	int chunk[4];
	int chunk_len = 0;

	while (i < input_len) {
		// Skip whitespace (per spec, base64 input allows ASCII whitespace)
		if (is_ascii_whitespace(input[i])) {
			i++;
			continue;
		}

		if (input[i] == '=') {
			// Padding — handle as part of chunk completion
			break;
		}

		// Per spec steps 10.8-10.9: check if adding this character would
		// complete a chunk that doesn't fit in the remaining output space
		size_t remaining = max_output - written;
		if (remaining == 1 && chunk_len == 2) {
			// A 3rd char would make a 3-char final chunk producing 2 bytes,
			// or lead to a 4-char chunk producing 3 bytes — neither fits in 1
			*out_read = read;
			*out_written = written;
			return 0;
		}
		if (remaining == 2 && chunk_len == 3) {
			// A 4th char would make a full chunk producing 3 bytes — doesn't fit in 2
			*out_read = read;
			*out_written = written;
			return 0;
		}

		int val = b64_char_value(input[i], url_safe);
		if (val < 0) {
			*out_read = read;
			*out_written = written;
			JS_ThrowSyntaxError(ctx, "Invalid base64 character");
			return -1;
		}

		chunk[chunk_len++] = val;
		i++;

		if (chunk_len == 4) {
			// Full chunk: produces 3 bytes
			if (written > SIZE_MAX - 3) {
				JS_ThrowRangeError(ctx, "base64 decoded output too large");
				return -1;
			}
			if (output) {
				output[written]     = (chunk[0] << 2) | (chunk[1] >> 4);
				output[written + 1] = ((chunk[1] & 0xF) << 4) | (chunk[2] >> 2);
				output[written + 2] = ((chunk[2] & 0x3) << 6) | chunk[3];
			}
			written += 3;
			chunk_len = 0;
			read = i;  // update read after successful full chunk
			if (written == max_output) {
				*out_read = read;
				*out_written = written;
				return 0;
			}
		}
	}

	// Handle padding characters
	if (i < input_len && input[i] == '=') {
		if (chunk_len < 2) {
			*out_read = read;
			*out_written = written;
			JS_ThrowSyntaxError(ctx, "Invalid base64: unexpected padding");
			return -1;
		}
		i++;
		// Skip whitespace between padding chars
		while (i < input_len && is_ascii_whitespace(input[i]))
			i++;
		if (chunk_len == 2) {
			// Need a second '='
			if (i >= input_len) {
				if (last_chunk_handling == LAST_CHUNK_STOP_BEFORE_PARTIAL) {
					*out_read = read;
					*out_written = written;
					return 0;
				}
				*out_read = read;
				*out_written = written;
				JS_ThrowSyntaxError(ctx, "Invalid base64: incomplete padding");
				return -1;
			}
			if (input[i] == '=') {
				i++;
				// Skip trailing whitespace
				while (i < input_len && is_ascii_whitespace(input[i]))
					i++;
			}
		} else {
			// chunk_len == 3, only one '=' needed
			// Skip trailing whitespace
			while (i < input_len && is_ascii_whitespace(input[i]))
				i++;
		}
		// After padding, must be at end of string
		if (i < input_len) {
			*out_read = read;
			*out_written = written;
			JS_ThrowSyntaxError(ctx, "Invalid base64: data after padding");
			return -1;
		}
		// Decode the padded final chunk
		int throw_on_extra = (last_chunk_handling == LAST_CHUNK_STRICT);
		if (chunk_len == 2) {
			if (throw_on_extra && (chunk[1] & 0xF) != 0) {
				*out_read = read;
				*out_written = written;
				JS_ThrowSyntaxError(ctx, "Invalid base64: non-zero overflow bits");
				return -1;
			}
			if (written > SIZE_MAX - 1) {
				JS_ThrowRangeError(ctx, "base64 decoded output too large");
				return -1;
			}
			if (output)
				output[written] = (chunk[0] << 2) | (chunk[1] >> 4);
			written += 1;
		} else {
			// chunk_len == 3
			if (throw_on_extra && (chunk[2] & 0x3) != 0) {
				*out_read = read;
				*out_written = written;
				JS_ThrowSyntaxError(ctx, "Invalid base64: non-zero overflow bits");
				return -1;
			}
			if (written > SIZE_MAX - 2) {
				JS_ThrowRangeError(ctx, "base64 decoded output too large");
				return -1;
			}
			if (output) {
				output[written]     = (chunk[0] << 2) | (chunk[1] >> 4);
				output[written + 1] = ((chunk[1] & 0xF) << 4) | (chunk[2] >> 2);
			}
			written += 2;
		}
		*out_read = i;
		*out_written = written;
		return 0;
	}

	// End of string with no padding — handle remaining partial chunk
	if (chunk_len == 0) {
		*out_read = i;
		*out_written = written;
		return 0;
	}

	if (last_chunk_handling == LAST_CHUNK_STOP_BEFORE_PARTIAL) {
		*out_read = read;
		*out_written = written;
		return 0;
	}

	if (chunk_len == 1) {
		*out_read = read;
		*out_written = written;
		JS_ThrowSyntaxError(ctx, "Invalid base64: incomplete chunk");
		return -1;
	}

	if (last_chunk_handling == LAST_CHUNK_STRICT) {
		*out_read = read;
		*out_written = written;
		JS_ThrowSyntaxError(ctx, "Invalid base64: missing padding");
		return -1;
	}

	// lastChunkHandling == "loose": decode partial chunk without checking extra bits
	if (chunk_len == 2) {
		if (written > SIZE_MAX - 1) {
			JS_ThrowRangeError(ctx, "base64 decoded output too large");
			return -1;
		}
		if (output)
			output[written] = (chunk[0] << 2) | (chunk[1] >> 4);
		written += 1;
	} else {
		// chunk_len == 3
		if (written > SIZE_MAX - 2) {
			JS_ThrowRangeError(ctx, "base64 decoded output too large");
			return -1;
		}
		if (output) {
			output[written]     = (chunk[0] << 2) | (chunk[1] >> 4);
			output[written + 1] = ((chunk[1] & 0xF) << 4) | (chunk[2] >> 2);
		}
		written += 2;
	}
	*out_read = i;
	*out_written = written;
	return 0;
}

// ---- toBase64([options]) ----

static JSValue nx_uint8array_to_base64(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	if (validate_uint8array(ctx, this_val) < 0) return JS_EXCEPTION;
	size_t offset, length;
	size_t buf_size;
	JSValue buf_val = JS_GetTypedArrayBuffer(ctx, this_val, &offset, &length, &buf_size);
	if (JS_IsException(buf_val))
		return JS_EXCEPTION;
	uint8_t *buf = JS_GetArrayBuffer(ctx, &buf_size, buf_val);
	JS_FreeValue(ctx, buf_val);
	if (!buf && length > 0)
		return JS_EXCEPTION;

	JSValueConst options = argc > 0 ? argv[0] : JS_UNDEFINED;
	int url_safe = parse_alphabet_option(ctx, options);
	if (url_safe < 0) return JS_EXCEPTION;
	int omit_padding = parse_omit_padding_option(ctx, options);
	if (omit_padding < 0) return JS_EXCEPTION;

	if (length == 0)
		return JS_NewString(ctx, "");

	uint8_t *data = buf + offset;

	// Encode using mbedtls for the standard case
	size_t b64_len = 0;
	int ret = mbedtls_base64_encode(NULL, 0, &b64_len, data, length);
	if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL && ret != 0)
		return JS_EXCEPTION;

	unsigned char *b64 = js_malloc(ctx, b64_len);
	if (!b64)
		return JS_EXCEPTION;

	ret = mbedtls_base64_encode(b64, b64_len, &b64_len, data, length);
	if (ret != 0) {
		js_free(ctx, b64);
		return JS_EXCEPTION;
	}

	size_t out_len = b64_len;

	if (omit_padding) {
		while (out_len > 0 && b64[out_len - 1] == '=')
			out_len--;
	}

	if (url_safe) {
		for (size_t i = 0; i < out_len; i++) {
			if (b64[i] == '+') b64[i] = '-';
			else if (b64[i] == '/') b64[i] = '_';
		}
	}

	JSValue result = JS_NewStringLen(ctx, (char *)b64, out_len);
	js_free(ctx, b64);
	return result;
}

// ---- toHex() ----

static JSValue nx_uint8array_to_hex(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	if (validate_uint8array(ctx, this_val) < 0) return JS_EXCEPTION;
	size_t offset, length;
	size_t buf_size;
	JSValue buf_val = JS_GetTypedArrayBuffer(ctx, this_val, &offset, &length, &buf_size);
	if (JS_IsException(buf_val))
		return JS_EXCEPTION;
	uint8_t *buf = JS_GetArrayBuffer(ctx, &buf_size, buf_val);
	JS_FreeValue(ctx, buf_val);
	if (!buf && length > 0)
		return JS_EXCEPTION;

	if (length == 0)
		return JS_NewString(ctx, "");

	uint8_t *data = buf + offset;

	if (length > SIZE_MAX / 2)
		return JS_ThrowRangeError(ctx, "Uint8Array too large to hex encode");

	size_t hex_len = length * 2;
	char *hex = js_malloc(ctx, hex_len);
	if (!hex) return JS_EXCEPTION;

	for (size_t i = 0; i < length; i++) {
		hex[i * 2]     = hex_chars[(data[i] >> 4) & 0xF];
		hex[i * 2 + 1] = hex_chars[data[i] & 0xF];
	}

	JSValue result = JS_NewStringLen(ctx, hex, hex_len);
	js_free(ctx, hex);
	return result;
}

// ---- Uint8Array.fromBase64(string[, options]) ----

static JSValue nx_uint8array_from_base64(JSContext *ctx, JSValueConst this_val,
										 int argc, JSValueConst *argv) {
	if (!JS_IsString(argv[0]))
		return JS_ThrowTypeError(ctx, "Expected string");
	size_t input_len;
	const char *input = JS_ToCStringLen(ctx, &input_len, argv[0]);
	if (!input)
		return JS_EXCEPTION;

	JSValueConst options = argc > 1 ? argv[1] : JS_UNDEFINED;
	int url_safe = parse_alphabet_option(ctx, options);
	if (url_safe < 0) { JS_FreeCString(ctx, input); return JS_EXCEPTION; }
	int last_chunk = parse_last_chunk_option(ctx, options);
	if (last_chunk < 0) { JS_FreeCString(ctx, input); return JS_EXCEPTION; }

	if (input_len == 0) {
		JS_FreeCString(ctx, input);
		return new_uint8array(ctx, NULL, 0);
	}

	// First pass: determine output size
	size_t read_count, written_count;
	int ret = base64_decode_impl(ctx, input, input_len, NULL, SIZE_MAX,
								 url_safe, last_chunk, &read_count, &written_count);
	if (ret != 0) {
		JS_FreeCString(ctx, input);
		return JS_EXCEPTION;
	}

	// Second pass: actually decode
	uint8_t *output = malloc(written_count > 0 ? written_count : 1);
	if (!output) {
		JS_FreeCString(ctx, input);
		return JS_ThrowOutOfMemory(ctx);
	}

	size_t r2, w2;
	ret = base64_decode_impl(ctx, input, input_len, output, SIZE_MAX,
							 url_safe, last_chunk, &r2, &w2);
	JS_FreeCString(ctx, input);

	if (ret != 0) {
		free(output);
		return JS_EXCEPTION;
	}

	return new_uint8array(ctx, output, w2);
}

// ---- Uint8Array.fromHex(string) ----

static JSValue nx_uint8array_from_hex(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	if (!JS_IsString(argv[0]))
		return JS_ThrowTypeError(ctx, "Expected string");
	size_t input_len;
	const char *input = JS_ToCStringLen(ctx, &input_len, argv[0]);
	if (!input)
		return JS_EXCEPTION;

	if (input_len % 2 != 0) {
		JS_FreeCString(ctx, input);
		return JS_ThrowSyntaxError(ctx, "Invalid hex string length");
	}

	if (input_len == 0) {
		JS_FreeCString(ctx, input);
		return new_uint8array(ctx, NULL, 0);
	}

	size_t output_len = input_len / 2;
	uint8_t *output = malloc(output_len);
	if (!output) {
		JS_FreeCString(ctx, input);
		return JS_ThrowOutOfMemory(ctx);
	}

	for (size_t i = 0; i < output_len; i++) {
		int hi = hex_digit(input[i * 2]);
		int lo = hex_digit(input[i * 2 + 1]);
		if (hi < 0 || lo < 0) {
			free(output);
			JS_FreeCString(ctx, input);
			return JS_ThrowSyntaxError(ctx, "Invalid hex character");
		}
		output[i] = (hi << 4) | lo;
	}

	JS_FreeCString(ctx, input);

	return new_uint8array(ctx, output, output_len);
}

// ---- setFromBase64(string[, options]) -> {read, written} ----

static JSValue nx_uint8array_set_from_base64(JSContext *ctx, JSValueConst this_val,
											 int argc, JSValueConst *argv) {
	if (validate_uint8array(ctx, this_val) < 0) return JS_EXCEPTION;
	if (!JS_IsString(argv[0]))
		return JS_ThrowTypeError(ctx, "Expected string");
	// Get the destination typed array buffer
	size_t offset, length;
	size_t buf_size;
	JSValue buf_val = JS_GetTypedArrayBuffer(ctx, this_val, &offset, &length, &buf_size);
	if (JS_IsException(buf_val))
		return JS_EXCEPTION;
	uint8_t *buf = JS_GetArrayBuffer(ctx, &buf_size, buf_val);
	JS_FreeValue(ctx, buf_val);
	if (!buf && length > 0)
		return JS_EXCEPTION;
	uint8_t *dest = buf ? buf + offset : NULL;

	size_t input_len;
	const char *input = JS_ToCStringLen(ctx, &input_len, argv[0]);
	if (!input)
		return JS_EXCEPTION;

	JSValueConst options = argc > 1 ? argv[1] : JS_UNDEFINED;
	int url_safe = parse_alphabet_option(ctx, options);
	if (url_safe < 0) { JS_FreeCString(ctx, input); return JS_EXCEPTION; }
	int last_chunk = parse_last_chunk_option(ctx, options);
	if (last_chunk < 0) { JS_FreeCString(ctx, input); return JS_EXCEPTION; }

	size_t read_count = 0, written_count = 0;
	int ret = base64_decode_impl(ctx, input, input_len, dest, length,
								 url_safe, last_chunk, &read_count, &written_count);
	JS_FreeCString(ctx, input);

	if (ret != 0)
		return JS_EXCEPTION;

	// Return { read, written }
	JSValue result = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, result, "read", JS_NewInt64(ctx, (int64_t)read_count));
	JS_SetPropertyStr(ctx, result, "written", JS_NewInt64(ctx, (int64_t)written_count));
	return result;
}

// ---- setFromHex(string) -> {read, written} ----

static JSValue nx_uint8array_set_from_hex(JSContext *ctx, JSValueConst this_val,
										  int argc, JSValueConst *argv) {
	if (validate_uint8array(ctx, this_val) < 0) return JS_EXCEPTION;
	if (!JS_IsString(argv[0]))
		return JS_ThrowTypeError(ctx, "Expected string");
	// Get the destination typed array buffer
	size_t offset, length;
	size_t buf_size;
	JSValue buf_val = JS_GetTypedArrayBuffer(ctx, this_val, &offset, &length, &buf_size);
	if (JS_IsException(buf_val))
		return JS_EXCEPTION;
	uint8_t *buf = JS_GetArrayBuffer(ctx, &buf_size, buf_val);
	JS_FreeValue(ctx, buf_val);
	if (!buf && length > 0)
		return JS_EXCEPTION;
	uint8_t *dest = buf ? buf + offset : NULL;

	size_t input_len;
	const char *input = JS_ToCStringLen(ctx, &input_len, argv[0]);
	if (!input)
		return JS_EXCEPTION;

	if (input_len % 2 != 0) {
		JS_FreeCString(ctx, input);
		return JS_ThrowSyntaxError(ctx, "Invalid hex string length");
	}

	size_t max_bytes = length;
	size_t hex_pairs = input_len / 2;
	size_t to_decode = hex_pairs < max_bytes ? hex_pairs : max_bytes;
	size_t written = 0;

	for (size_t i = 0; i < to_decode; i++) {
		int hi = hex_digit(input[i * 2]);
		int lo = hex_digit(input[i * 2 + 1]);
		if (hi < 0 || lo < 0) {
			JS_FreeCString(ctx, input);
			return JS_ThrowSyntaxError(ctx, "Invalid hex character");
		}
		dest[i] = (hi << 4) | lo;
		written++;
	}

	size_t read_count = written * 2;
	JS_FreeCString(ctx, input);

	// Return { read, written }
	JSValue result = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, result, "read", JS_NewInt64(ctx, (int64_t)read_count));
	JS_SetPropertyStr(ctx, result, "written", JS_NewInt64(ctx, (int64_t)written));
	return result;
}

// ---- Init ----

static JSValue nx_uint8array_init(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	// Store a reference to the Uint8Array constructor for fromBase64/fromHex
	uint8array_ctor = JS_DupValue(ctx, argv[0]);

	// Define static methods on the constructor
	NX_DEF_FUNC(argv[0], "fromBase64", nx_uint8array_from_base64, 1);
	NX_DEF_FUNC(argv[0], "fromHex", nx_uint8array_from_hex, 1);

	// Define instance methods on the prototype
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_FUNC(proto, "toBase64", nx_uint8array_to_base64, 0);
	NX_DEF_FUNC(proto, "toHex", nx_uint8array_to_hex, 0);
	NX_DEF_FUNC(proto, "setFromBase64", nx_uint8array_set_from_base64, 1);
	NX_DEF_FUNC(proto, "setFromHex", nx_uint8array_set_from_hex, 1);
	JS_FreeValue(ctx, proto);

	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("uint8arrayInit", 1, nx_uint8array_init),
};

void nx_init_uint8array(JSContext *ctx, JSValueConst init_obj) {
	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
