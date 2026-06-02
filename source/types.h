#pragma once

// nx.js native types — V8 + libuv era.
//
// See BINDINGS.md for the full house style. Key points reflected here:
//   - The engine is V8 (C++); every native module is compiled as C++ (.cc).
//   - The event loop + threadpool + async I/O are libuv (uv_loop_t / uv_work_t).
//   - QuickJS, wasm3, the hand-rolled poll.c/thpool.c are gone.
//   - Phase 1 keeps Cairo for rendering (swapped to Skia in Phase 2).

#include <cairo-ft.h>
#include <ft2build.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/x509_crt.h>
#include <stdbool.h>
#include <stdint.h>
#include <switch.h>
#include <uv.h>
#include <v8.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef countof
#define countof(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef NXJS_VERSION
#define NXJS_VERSION "0.0.0"
#endif

// ---------------------------------------------------------------------------
// Binding helper macros (the ONLY sanctioned macros; everything else is raw
// v8::). Each assumes `v8::Isolate *iso` is in scope. See BINDINGS.md §2–§3.
// ---------------------------------------------------------------------------

// Interned ("internalized") UTF-8 string literal -> Local<String>.
static inline v8::Local<v8::String> nx_str(v8::Isolate *iso, const char *s) {
	return v8::String::NewFromUtf8(iso, s, v8::NewStringType::kInternalized)
	    .ToLocalChecked();
}

// Attach a native function `FN` onto object `OBJ` under property `NAME`.
// Used by nx_init_*() to populate the `$` bridge object.
#define NX_SET_FUNC(OBJ, NAME, FN)                                             \
	do {                                                                       \
		v8::Local<v8::Context> _ctx = iso->GetCurrentContext();                \
		(OBJ)->Set(_ctx, nx_str(iso, NAME),                                    \
		           v8::FunctionTemplate::New(iso, FN)                          \
		               ->GetFunction(_ctx)                                     \
		               .ToLocalChecked())                                      \
		    .Check();                                                          \
	} while (0)

// Define a getter-only accessor `FN` on prototype object `PROTO` as `NAME`.
#define NX_DEF_GET(PROTO, NAME, FN)                                            \
	do {                                                                       \
		v8::Local<v8::Context> _ctx = iso->GetCurrentContext();                \
		v8::Local<v8::Function> _get =                                         \
		    v8::FunctionTemplate::New(iso, FN)->GetFunction(_ctx)              \
		        .ToLocalChecked();                                             \
		(PROTO)->SetAccessorProperty(nx_str(iso, NAME), _get);                 \
	} while (0)

// Define a getter+setter accessor pair on prototype object `PROTO` as `NAME`.
#define NX_DEF_GETSET(PROTO, NAME, GET_FN, SET_FN)                             \
	do {                                                                       \
		v8::Local<v8::Context> _ctx = iso->GetCurrentContext();                \
		v8::Local<v8::Function> _get =                                         \
		    v8::FunctionTemplate::New(iso, GET_FN)->GetFunction(_ctx)          \
		        .ToLocalChecked();                                             \
		v8::Local<v8::Function> _set =                                         \
		    v8::FunctionTemplate::New(iso, SET_FN)->GetFunction(_ctx)          \
		        .ToLocalChecked();                                             \
		(PROTO)->SetAccessorProperty(nx_str(iso, NAME), _get, _set);           \
	} while (0)

// Define a method `FN` (arity LENGTH) on prototype object `PROTO` as `NAME`.
#define NX_DEF_FUNC(PROTO, NAME, FN, LENGTH)                                   \
	do {                                                                       \
		v8::Local<v8::Context> _ctx = iso->GetCurrentContext();                \
		v8::Local<v8::Function> _fn =                                          \
		    v8::FunctionTemplate::New(iso, FN, v8::Local<v8::Value>(),         \
		                              v8::Local<v8::Signature>(), LENGTH)      \
		        ->GetFunction(_ctx)                                            \
		        .ToLocalChecked();                                             \
		(PROTO)->Set(_ctx, nx_str(iso, NAME), _fn).Check();                    \
	} while (0)

// ---------------------------------------------------------------------------
// Async / threadpool over libuv. See BINDINGS.md §6.
// ---------------------------------------------------------------------------

struct nx_work_s;
typedef struct nx_work_s nx_work_t;

// Runs on a libuv worker thread. MUST NOT touch any V8 API. Reads inputs from
// and writes outputs to `req` (typically `req->data`).
typedef void (*nx_work_cb)(nx_work_t *req);

// Runs on the loop thread inside a HandleScope + Context::Scope. Builds the
// result value to resolve the promise with. If it leaves an exception pending
// (returns an empty MaybeLocal), the promise is rejected with that exception.
typedef v8::MaybeLocal<v8::Value> (*nx_after_work_cb)(v8::Isolate *iso,
                                                      nx_work_t *req);

// Optional destructor for `req->data`. When set, the framework calls this
// (instead of free()) after `after_work_cb` runs. Use this for payloads that
// contain non-trivial C++ members (e.g. v8::Global<>) which must be properly
// destructed rather than free()'d. Runs on the loop thread.
typedef void (*nx_data_dtor)(void *data);

struct nx_work_s {
	uv_work_t req; // first member: uv hands this struct back to us
	v8::Isolate *iso;
	v8::Global<v8::Promise::Resolver> resolver;
	nx_work_cb work_cb;
	nx_after_work_cb after_work_cb;
	bool failed;          // set by work_cb to force rejection
	void *data;           // module-specific payload (freed by the framework)
	nx_data_dtor data_dtor; // if non-null, called to destroy `data`
	                        // instead of free()
};

// ---------------------------------------------------------------------------
// Per-isolate global state (replaces the QuickJS opaque context).
// Stored via iso->SetData(0, nx_ctx); fetched with nx_ctx(iso).
// ---------------------------------------------------------------------------

enum nx_rendering_mode {
	NX_RENDERING_MODE_INIT,
	NX_RENDERING_MODE_CONSOLE,
	NX_RENDERING_MODE_CANVAS
};

typedef struct nx_context_s {
	// `had_error` is only written/read on the main (loop) thread.
	int had_error;
	enum nx_rendering_mode rendering_mode;

	uv_loop_t *loop; // the libuv event loop (owned by main.cc)
	v8::Isolate *iso;

	FT_Library ft_library;
	HidVibrationDeviceHandle vibration_device_handles[2];

	// The `$` bridge object and the retained JS handlers.
	v8::Global<v8::Object> init_obj;
	v8::Global<v8::Function> frame_handler;
	v8::Global<v8::Function> exit_handler;
	v8::Global<v8::Function> error_handler;
	v8::Global<v8::Function> unhandled_rejection_handler;
	v8::Global<v8::Promise> unhandled_rejected_promise;

	PadState pads[8];

	// mbedtls structures shared by all TLS connections
	bool mbedtls_initialized;
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;

	// System CA certificate chain (loaded lazily from Switch SSL service)
	bool ca_certs_loaded;
	mbedtls_x509_crt ca_chain;

	bool spl_initialized;
} nx_context_t;

// Fetch the per-isolate context (replaces JS_GetContextOpaque).
static inline nx_context_t *nx_ctx(v8::Isolate *iso) {
	return static_cast<nx_context_t *>(iso->GetData(0));
}
