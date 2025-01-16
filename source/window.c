#include "window.h"

static JSValue nx_atob(JSContext *ctx, JSValueConst this_val, int argc,
					   JSValueConst *argv) {
	size_t input_len;
	const char *input = JS_ToCStringLen(ctx, &input_len, argv[0]);
	if (!input)
		return JS_EXCEPTION;

	// Calculate decoded length (removing padding)
	size_t padding = 0;
	if (input_len > 0 && input[input_len - 1] == '=')
		padding++;
	if (input_len > 1 && input[input_len - 2] == '=')
		padding++;
	size_t output_len = (input_len * 3) / 4 - padding;

	// Allocate output buffer
	uint8_t *output = js_malloc(ctx, output_len + 1);
	if (!output) {
		JS_FreeCString(ctx, input);
		return JS_EXCEPTION;
	}

	// Base64 decoding lookup table
	static const int8_t b64_table[256] = {
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, // 0-15
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, // 16-31
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, 62, -1, -1, -1, 63, // 32-47
		52, 53, 54, 55, 56, 57, 58, 59,
		60, 61, -1, -1, -1, -1, -1, -1, // 48-63
		-1, 0,  1,  2,  3,  4,  5,  6,
		7,  8,  9,  10, 11, 12, 13, 14, // 64-79
		15, 16, 17, 18, 19, 20, 21, 22,
		23, 24, 25, -1, -1, -1, -1, -1, // 80-95
		-1, 26, 27, 28, 29, 30, 31, 32,
		33, 34, 35, 36, 37, 38, 39, 40, // 96-111
		41, 42, 43, 44, 45, 46, 47, 48,
		49, 50, 51, -1, -1, -1, -1, -1 // 112-127
	};

	// Decode
	size_t i = 0, j = 0;
	uint32_t accum = 0;
	int bits = 0;

	while (i < input_len) {
		int c = input[i++];
		int x = b64_table[c & 0x7F];

		if (x == -1) {
			js_free(ctx, output);
			JS_FreeCString(ctx, input);
			return JS_ThrowSyntaxError(ctx, "Invalid base64 character");
		}

		accum = (accum << 6) | x;
		bits += 6;

		if (bits >= 8) {
			bits -= 8;
			output[j++] = (accum >> bits) & 0xFF;
		}
	}

	output[output_len] = 0;
	JS_FreeCString(ctx, input);

	JSValue result = JS_NewStringLen(ctx, (char *)output, output_len);
	js_free(ctx, output);
	return result;
}

static JSValue nx_btoa(JSContext *ctx, JSValueConst this_val, int argc,
					   JSValueConst *argv) {
	const char *input = JS_ToCString(ctx, argv[0]);
	if (!input) {
		return JS_EXCEPTION;
	}

	size_t input_len = strlen(input);
	size_t output_len = ((input_len + 2) / 3) * 4;
	unsigned char *output = js_malloc(ctx, output_len + 1);
	if (!output) {
		JS_FreeCString(ctx, input);
		return JS_EXCEPTION;
	}

	static const char b64_chars[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	// Encode
	size_t i = 0, j = 0;
	uint32_t accum = 0;
	int bits = 0;

	while (i < input_len) {
		accum = (accum << 8) | (input[i++] & 0xFF);
		bits += 8;

		while (bits >= 6) {
			bits -= 6;
			output[j++] = b64_chars[(accum >> bits) & 0x3F];
		}
	}

	// Handle remaining bits
	if (bits > 0) {
		accum <<= (6 - bits);
		output[j++] = b64_chars[accum & 0x3F];
	}

	// Add padding
	while (j < output_len) {
		output[j++] = '=';
	}

	output[output_len] = 0;
	JS_FreeCString(ctx, input);

	JSValue result = JS_NewString(ctx, (char *)output);
	js_free(ctx, output);
	return result;
}

static JSValue nx_window_init(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	NX_DEF_FUNC(argv[0], "atob", nx_atob, 1);
	NX_DEF_FUNC(argv[0], "btoa", nx_btoa, 1);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("windowInit", 1, nx_window_init),
};

void nx_init_window(JSContext *ctx, JSValueConst init_obj) {
	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
