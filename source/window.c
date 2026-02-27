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
