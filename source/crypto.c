#include <switch.h>
#include "crypto.h"

static JSValue nx_crypto_random_bytes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	size_t size;
	void *buf = JS_GetArrayBuffer(ctx, &size, argv[0]);

	int offset;
	int length;
	if (JS_ToInt32(ctx, &offset, argv[1]) || JS_ToInt32(ctx, &length, argv[2]) || offset + length > size)
	{
		JS_ThrowTypeError(ctx, "invalid input");
		return JS_EXCEPTION;
	}
	randomGet(buf + offset, length);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("cryptoRandomBytes", 0, nx_crypto_random_bytes)};

void nx_init_crypto(JSContext *ctx, JSValueConst native_obj)
{
	JS_SetPropertyFunctionList(ctx, native_obj, function_list, countof(function_list));
}
