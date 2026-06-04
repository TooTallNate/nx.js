#pragma once
#include "types.h"

// Print the currently-caught exception (from a TryCatch) to stdout + stderr.
void print_js_error(v8::Isolate *iso, v8::TryCatch *try_catch);

// Throw helpers (schedule the exception on the isolate; caller must `return`).
void nx_throw(v8::Isolate *iso, const char *message);
void nx_throw_errno_error(v8::Isolate *iso, int err, const char *syscall);

// Throw a RangeError describing an out-of-memory condition. Caller must
// `return` (the exception is scheduled on the isolate). Used by nx_alloc().
void nx_throw_oom(v8::Isolate *iso, size_t size);

// malloc() that, on failure, schedules a RangeError on the isolate and returns
// NULL. Callers MUST check for NULL and `return` (the pending exception then
// surfaces as a normal JS error instead of a NULL-deref crash). NEVER aborts.
void *nx_alloc(v8::Isolate *iso, size_t size);

// Build a Local<String> from UTF-8 bytes that may NOT be well-formed (e.g.
// filesystem names, account nicknames, IPC payloads). Unlike nx_str()/
// String::NewFromUtf8(...).ToLocalChecked(), this NEVER aborts: if the bytes are
// not valid UTF-8 it falls back to a byte-for-byte (Latin-1) string so a
// malformed name can't crash the console. `len` < 0 means NUL-terminated.
v8::Local<v8::String> nx_str_lossy(v8::Isolate *iso, const char *s, int len = -1);

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
