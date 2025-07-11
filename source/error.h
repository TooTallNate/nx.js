#pragma once
#include "types.h"

void print_js_error(JSContext *ctx);

JSValue nx_throw_errno_error(JSContext *ctx, int errno, char *syscall);

// Error codes explanations:
//  - https://switchbrew.org/wiki/Error_codes
//  - https://github.com/backupbrew/switchbrew/blob/master/Error%20codes.md
//  - https://github.com/switchbrew/libnx/blob/master/nx/include/switch/result.h
JSValue nx_throw_libnx_error(JSContext *ctx, Result rc, char *name);

void nx_emit_error_event(JSContext *ctx);
void nx_emit_unhandled_rejection_event(JSContext *ctx);

void nx_promise_rejection_handler(JSContext *ctx, JSValueConst promise,
								  JSValueConst reason, bool is_handled,
								  void *opaque);

void nx_init_error(JSContext *ctx, JSValueConst init_obj);
