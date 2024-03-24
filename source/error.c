#include "error.h"

void print_js_error(JSContext *ctx)
{
	JSValue exception_val = JS_GetException(ctx);
	const char *exception_str = JS_ToCString(ctx, exception_val);
	printf("%s\n", exception_str);
	fprintf(stderr, "%s\n", exception_str);
	JS_FreeCString(ctx, exception_str);

	JSValue stack_val = JS_GetPropertyStr(ctx, exception_val, "stack");
	const char *stack_str = JS_ToCString(ctx, stack_val);
	printf("%s\n", stack_str);
	fprintf(stderr, "%s\n", stack_str);

	JS_FreeCString(ctx, stack_str);
	JS_FreeValue(ctx, exception_val);
	JS_FreeValue(ctx, stack_val);
}

JSValue nx_throw_libnx_error(JSContext *ctx, Result rc, char *name)
{
	JSValue err = JS_NewError(ctx);
	u32 module = R_MODULE(rc);
	u32 desc = R_DESCRIPTION(rc);
	char message[256];
	snprintf(message, 256, "%s failed (module: %u, description: %u)", name, module, desc);
	JS_DefinePropertyValueStr(ctx, err, "message", JS_NewString(ctx, message), JS_PROP_C_W);
	JS_SetPropertyStr(ctx, err, "module", JS_NewUint32(ctx, module));
	JS_SetPropertyStr(ctx, err, "description", JS_NewUint32(ctx, desc));
	JS_SetPropertyStr(ctx, err, "value", JS_NewUint32(ctx, R_VALUE(rc)));
	return JS_Throw(ctx, err);
}

void nx_emit_error_event(JSContext *ctx)
{
	JSValue exception_val = JS_GetException(ctx);
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);

	// Print the error to stderr so that it ends up in the log file
	const char *exception_str = JS_ToCString(ctx, exception_val);
	fprintf(stderr, "Uncaught %s\n", exception_str);
	JS_FreeCString(ctx, exception_str);

	JSValue stack_val = JS_GetPropertyStr(ctx, exception_val, "stack");
	if (!JS_IsUndefined(stack_val))
	{
		const char *stack_str = JS_ToCString(ctx, stack_val);
		fprintf(stderr, "%s\n", stack_str);
		JS_FreeCString(ctx, stack_str);
		JS_FreeValue(ctx, stack_val);
	}

	JSValueConst args[] = {exception_val};
	JSValue ret_val = JS_Call(ctx, nx_ctx->error_handler, JS_NULL, 1, args);
	if (JS_IsException(ret_val))
		print_js_error(ctx);
	JS_ToInt32(ctx, &nx_ctx->had_error, ret_val);

	JS_FreeValue(ctx, ret_val);
	JS_FreeValue(ctx, exception_val);
}

static JSValue nx_set_error_handler(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	nx_ctx->error_handler = JS_DupValue(ctx, argv[0]);
	return JS_UNDEFINED;
}

static JSValue nx_set_unhandled_rejection_handler(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	nx_ctx->unhandled_rejection_handler = JS_DupValue(ctx, argv[0]);
	return JS_UNDEFINED;
}

void nx_promise_rejection_handler(JSContext *ctx, JSValueConst promise,
								  JSValueConst reason,
								  JS_BOOL is_handled, void *opaque)
{
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);

	// Print the error to stderr so that it ends up in the log file
	const char *exception_str = JS_ToCString(ctx, reason);
	fprintf(stderr, "Uncaught (in promise) %s\n", exception_str);
	JS_FreeCString(ctx, exception_str);

	JSValue stack_val = JS_GetPropertyStr(ctx, reason, "stack");
	if (!JS_IsUndefined(stack_val))
	{
		const char *stack_str = JS_ToCString(ctx, stack_val);
		fprintf(stderr, "%s\n", stack_str);
		JS_FreeCString(ctx, stack_str);
		JS_FreeValue(ctx, stack_val);
	}

	JSValueConst args[] = {promise, reason};
	JSValue ret_val = JS_Call(ctx, nx_ctx->unhandled_rejection_handler, JS_NULL, 2, args);
	if (JS_IsException(ret_val))
		print_js_error(ctx);
	JS_ToInt32(ctx, &nx_ctx->had_error, ret_val);
	JS_FreeValue(ctx, ret_val);
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("onError", 1, nx_set_error_handler),
	JS_CFUNC_DEF("onUnhandledRejection", 1, nx_set_unhandled_rejection_handler),
};

void nx_init_error(JSContext *ctx, JSValueConst init_obj)
{
	JS_SetPropertyFunctionList(ctx, init_obj, function_list, countof(function_list));
}
