/**
 * Stub implementations for functions referenced by crypto.c
 * that normally live in other nx.js source files or libnx.
 */

#include "types.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

// ---- error.c stubs ----

void print_js_error(JSContext *ctx) {
	JSValue exception_val = JS_GetException(ctx);
	const char *exception_str = JS_ToCString(ctx, exception_val);
	if (exception_str) {
		fprintf(stderr, "%s\n", exception_str);
		JS_FreeCString(ctx, exception_str);
	}
	JSValue stack_val = JS_GetPropertyStr(ctx, exception_val, "stack");
	if (!JS_IsUndefined(stack_val)) {
		const char *stack_str = JS_ToCString(ctx, stack_val);
		if (stack_str) {
			fprintf(stderr, "%s\n", stack_str);
			JS_FreeCString(ctx, stack_str);
		}
	}
	JS_FreeValue(ctx, stack_val);
	JS_FreeValue(ctx, exception_val);
}

JSValue nx_throw_libnx_error(JSContext *ctx, Result rc, char *name) {
	char message[256];
	snprintf(message, sizeof(message),
	         "%s failed (libnx error 0x%x — not available on host)", name, rc);
	return JS_ThrowInternalError(ctx, "%s", message);
}

JSValue nx_throw_errno_error(JSContext *ctx, int err, char *syscall) {
	return JS_ThrowInternalError(ctx, "%s (%s)", strerror(err), syscall);
}

void nx_emit_error_event(JSContext *ctx) {
	JSValue exception_val = JS_GetException(ctx);
	const char *str = JS_ToCString(ctx, exception_val);
	if (str) {
		fprintf(stderr, "Uncaught %s\n", str);
		JS_FreeCString(ctx, str);
	}
	JS_FreeValue(ctx, exception_val);
}

void nx_emit_unhandled_rejection_event(JSContext *ctx) { (void)ctx; }

void nx_promise_rejection_handler(JSContext *ctx, JSValueConst promise,
                                  JSValueConst reason, bool is_handled,
                                  void *opaque) {
	(void)ctx; (void)promise; (void)reason; (void)is_handled; (void)opaque;
}

void nx_init_error(JSContext *ctx, JSValueConst init_obj) {
	(void)ctx; (void)init_obj;
}

// ---- async.c stubs (synchronous execution) ----

void nx_process_async(JSContext *ctx, nx_context_t *nx_ctx) {
	(void)ctx; (void)nx_ctx;
}

JSValue nx_queue_async(JSContext *ctx, nx_work_t *req, nx_work_cb work_cb,
                       nx_after_work_cb after_work_cb) {
	(void)req;
	work_cb(req);
	JSValue result = after_work_cb(ctx, req);

	JSValue promise, resolving_funcs[2];
	promise = JS_NewPromiseCapability(ctx, resolving_funcs);

	JSValue args[1];
	if (JS_IsException(result)) {
		args[0] = JS_GetException(ctx);
		JS_Call(ctx, resolving_funcs[1], JS_NULL, 1, args);
		JS_FreeValue(ctx, args[0]);
	} else {
		args[0] = result;
		JS_Call(ctx, resolving_funcs[0], JS_NULL, 1, args);
		JS_FreeValue(ctx, args[0]);
	}
	JS_FreeValue(ctx, resolving_funcs[0]);
	JS_FreeValue(ctx, resolving_funcs[1]);

	if (req->data)
		free(req->data);
	free(req);

	return promise;
}

// ---- util.c (NX_GetBufferSource) ----

u8 *NX_GetBufferSource(JSContext *ctx, size_t *size, JSValueConst obj) {
	if (!JS_IsObject(obj)) {
		return NULL;
	}
	if (JS_IsArrayBuffer(obj)) {
		return JS_GetArrayBuffer(ctx, size, obj);
	}
	JSValue buffer_val = JS_GetPropertyStr(ctx, obj, "buffer");
	if (!JS_IsArrayBuffer(buffer_val)) {
		return NULL;
	}
	u32 byte_offset;
	u32 byte_length;
	JSValue byte_offset_val = JS_GetPropertyStr(ctx, obj, "byteOffset");
	JSValue byte_length_val = JS_GetPropertyStr(ctx, obj, "byteLength");
	if (JS_ToUint32(ctx, &byte_offset, byte_offset_val) ||
	    JS_ToUint32(ctx, &byte_length, byte_length_val)) {
		return NULL;
	}
	size_t ab_size;
	u8 *ptr = JS_GetArrayBuffer(ctx, &ab_size, buffer_val);
	if (!ptr) {
		return NULL;
	}
	*size = byte_length;
	return ptr + byte_offset;
}
