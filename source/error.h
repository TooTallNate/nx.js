#pragma once
#include "types.h"

void print_js_error(JSContext *ctx);

// Error codes explanations:
//  - https://switchbrew.org/wiki/Error_codes
//  - https://github.com/backupbrew/switchbrew/blob/master/Error%20codes.md
JSValue nx_throw_libnx_error(JSContext *ctx, Result rc, char *name);

void nx_emit_error_event(JSContext *ctx);

void nx_promise_rejection_handler(JSContext *ctx, JSValueConst promise,
								  JSValueConst reason, JS_BOOL is_handled,
								  void *opaque);

void nx_init_error(JSContext *ctx, JSValueConst init_obj);
