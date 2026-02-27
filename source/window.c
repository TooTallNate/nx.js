#include "window.h"
#include <mbedtls/base64.h>

static JSValue nx_atob(JSContext *ctx, JSValueConst this_val, int argc,
					   JSValueConst *argv) {
	size_t input_len;
	const char *input = JS_ToCStringLen(ctx, &input_len, argv[0]);
	if (!input)
		return JS_EXCEPTION;

	// Determine required output buffer size
	size_t output_len = 0;
	int ret = mbedtls_base64_decode(NULL, 0, &output_len,
									(const unsigned char *)input, input_len);
	if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL && ret != 0) {
		JS_FreeCString(ctx, input);
		return JS_ThrowSyntaxError(ctx, "Invalid base64 character");
	}

	uint8_t *output = js_malloc(ctx, output_len + 1);
	if (!output) {
		JS_FreeCString(ctx, input);
		return JS_EXCEPTION;
	}

	ret = mbedtls_base64_decode(output, output_len, &output_len,
								(const unsigned char *)input, input_len);
	JS_FreeCString(ctx, input);

	if (ret != 0) {
		js_free(ctx, output);
		return JS_ThrowSyntaxError(ctx, "Invalid base64 character");
	}

	output[output_len] = 0;
	JSValue result = JS_NewStringLen(ctx, (char *)output, output_len);
	js_free(ctx, output);
	return result;
}

static JSValue nx_btoa(JSContext *ctx, JSValueConst this_val, int argc,
					   JSValueConst *argv) {
	size_t input_len;
	const char *input = JS_ToCStringLen(ctx, &input_len, argv[0]);
	if (!input)
		return JS_EXCEPTION;

	// Determine required output buffer size
	size_t output_len = 0;
	int ret = mbedtls_base64_encode(NULL, 0, &output_len,
									(const unsigned char *)input, input_len);
	if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL && ret != 0) {
		JS_FreeCString(ctx, input);
		return JS_EXCEPTION;
	}

	unsigned char *output = js_malloc(ctx, output_len);
	if (!output) {
		JS_FreeCString(ctx, input);
		return JS_EXCEPTION;
	}

	ret = mbedtls_base64_encode(output, output_len, &output_len,
								(const unsigned char *)input, input_len);
	JS_FreeCString(ctx, input);

	if (ret != 0) {
		js_free(ctx, output);
		return JS_EXCEPTION;
	}

	JSValue result = JS_NewStringLen(ctx, (char *)output, output_len);
	js_free(ctx, output);
	return result;
}

static JSValue nx_base64url_encode(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	size_t input_len;
	uint8_t *input = JS_GetArrayBuffer(ctx, &input_len, argv[0]);
	if (!input)
		return JS_EXCEPTION;

	// Determine required output buffer size for standard base64
	size_t output_len = 0;
	int ret = mbedtls_base64_encode(NULL, 0, &output_len, input, input_len);
	if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL && ret != 0)
		return JS_EXCEPTION;

	unsigned char *output = js_malloc(ctx, output_len + 1);
	if (!output)
		return JS_EXCEPTION;

	ret = mbedtls_base64_encode(output, output_len + 1, &output_len, input,
								input_len);
	if (ret != 0) {
		js_free(ctx, output);
		return JS_EXCEPTION;
	}

	// Convert to base64url: + -> -, / -> _, strip =
	size_t final_len = output_len;
	for (size_t i = 0; i < final_len; i++) {
		if (output[i] == '+')
			output[i] = '-';
		else if (output[i] == '/')
			output[i] = '_';
	}
	// Strip trailing '='
	while (final_len > 0 && output[final_len - 1] == '=')
		final_len--;

	JSValue result = JS_NewStringLen(ctx, (char *)output, final_len);
	js_free(ctx, output);
	return result;
}

static JSValue nx_base64url_decode(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	size_t input_len;
	const char *input = JS_ToCStringLen(ctx, &input_len, argv[0]);
	if (!input)
		return JS_EXCEPTION;

	// Convert from base64url to standard base64: - -> +, _ -> /
	// Add padding
	size_t padded_len = input_len;
	size_t remainder = padded_len % 4;
	if (remainder == 2)
		padded_len += 2;
	else if (remainder == 3)
		padded_len += 1;

	char *b64 = js_malloc(ctx, padded_len + 1);
	if (!b64) {
		JS_FreeCString(ctx, input);
		return JS_EXCEPTION;
	}

	for (size_t i = 0; i < input_len; i++) {
		if (input[i] == '-')
			b64[i] = '+';
		else if (input[i] == '_')
			b64[i] = '/';
		else
			b64[i] = input[i];
	}
	for (size_t i = input_len; i < padded_len; i++)
		b64[i] = '=';
	b64[padded_len] = 0;

	JS_FreeCString(ctx, input);

	// Decode
	size_t output_len = 0;
	int ret = mbedtls_base64_decode(NULL, 0, &output_len,
									(const unsigned char *)b64, padded_len);
	if (ret != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL && ret != 0) {
		js_free(ctx, b64);
		return JS_ThrowSyntaxError(ctx, "Invalid base64url character");
	}

	uint8_t *output = js_malloc(ctx, output_len);
	if (!output) {
		js_free(ctx, b64);
		return JS_EXCEPTION;
	}

	ret = mbedtls_base64_decode(output, output_len, &output_len,
								(const unsigned char *)b64, padded_len);
	js_free(ctx, b64);

	if (ret != 0) {
		js_free(ctx, output);
		return JS_ThrowSyntaxError(ctx, "Invalid base64url character");
	}

	JSValue result = JS_NewArrayBufferCopy(ctx, output, output_len);
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
	JS_CFUNC_DEF("base64urlEncode", 1, nx_base64url_encode),
	JS_CFUNC_DEF("base64urlDecode", 1, nx_base64url_decode),
};

void nx_init_window(JSContext *ctx, JSValueConst init_obj) {
	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
