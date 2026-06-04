#pragma once
#include "types.h"

// Allocate a zeroed nx_work_t plus a zeroed typed `data` payload.
// `req` and `data` are in scope after this macro (see BINDINGS.md §6).
// Use this for trivially-copyable (POD) payloads; the framework free()s them.
#define NX_INIT_WORK_T(type)                                                   \
	nx_work_t *req = new nx_work_t();                                          \
	type *data = static_cast<type *>(calloc(1, sizeof(type)));                 \
	req->data = data;

// Like NX_INIT_WORK_T but C++-constructs the payload with `new` and registers
// a destructor so the framework `delete`s it (calling member destructors).
// Use this for payloads containing non-trivial members (e.g. v8::Global<>).
#define NX_INIT_WORK_T_CPP(type)                                               \
	nx_work_t *req = new nx_work_t();                                          \
	type *data = new type();                                                   \
	req->data = data;                                                          \
	req->data_dtor = [](void *p) { delete static_cast<type *>(p); };

// Create a Promise, queue `work_cb` on the libuv threadpool, and return the
// promise to JS synchronously. When the work finishes, `after_work_cb` runs on
// the loop thread to build the resolution value (or leaves an exception pending
// to reject). The framework frees `req->data` and `req`.
//
// Must be called on the loop thread with a HandleScope + entered Context.
v8::Local<v8::Promise> nx_queue_async(v8::Isolate *iso, nx_work_t *req,
                                      nx_work_cb work_cb,
                                      nx_after_work_cb after_work_cb);
