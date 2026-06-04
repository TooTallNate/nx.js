#pragma once
#include "types.h"

// Print the currently-caught exception (from a TryCatch) to stdout + stderr.
void print_js_error(v8::Isolate *iso, v8::TryCatch *try_catch);

// Throw helpers (schedule the exception on the isolate; caller must `return`).
void nx_throw(v8::Isolate *iso, const char *message);
void nx_throw_errno_error(v8::Isolate *iso, int err, const char *syscall);

// Error codes explanations:
//  - https://switchbrew.org/wiki/Error_codes
//  - https://github.com/switchbrew/libnx/blob/master/nx/include/switch/result.h
void nx_throw_libnx_error(v8::Isolate *iso, Result rc, const char *name);

// Dispatch a caught exception (from `try_catch`) to the JS error handler.
void nx_emit_error_event(v8::Isolate *iso, v8::TryCatch *try_catch);

// Dispatch the stored unhandled-rejected promise to the JS rejection handler.
void nx_emit_unhandled_rejection_event(v8::Isolate *iso);

// V8 promise-reject callback (registered via Isolate::SetPromiseRejectCallback).
void nx_promise_rejection_handler(v8::PromiseRejectMessage message);

void nx_init_error(v8::Isolate *iso, v8::Local<v8::Object> init_obj);
