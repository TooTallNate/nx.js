#pragma once
#include "types.h"

void print_js_error(JSContext *ctx);

int nx_emit_error_event(JSContext *ctx);

void nx_promise_rejection_handler(JSContext *ctx, JSValueConst promise,
                                  JSValueConst reason,
                                  JS_BOOL is_handled, void *opaque);

void nx_init_error(JSContext *ctx, JSValueConst init_obj);