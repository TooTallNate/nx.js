#pragma once
#include "types.h"

// `hid:sys` service integration used to resolve a real, hardware-backed
// `Gamepad.id` (the controller's serial number + a human-readable device name)
// and to drive `gamepadconnected` / `gamepaddisconnected` invalidation.
//
// All hidsys IPC is kept OUT of the per-frame input-polling hot path: id
// strings are resolved lazily on first `Gamepad.id` access and cached per Npad
// index on `nx_context_t`. The cache is invalidated only when the OS signals a
// controller connect/disconnect (a single non-blocking event check per frame).

// Open the `hid:sys` service and acquire the unique-pad connection event.
// Tolerates failure (sets nx_ctx->hidsys_available = false) — `Gamepad.id`
// then falls back to the index-based string. Call once during service init.
void nx_hidsys_init(v8::Isolate *iso);

// Close the connection event + the `hid:sys` service. Call during teardown.
// Takes the context directly (not the isolate) because teardown runs it after
// the isolate has been disposed.
void nx_hidsys_exit(nx_context_t *ctx);

// Resolve the id string for Npad `index` into `out` (size `out_len`), using the
// per-index cache. On a cache miss it performs the hidsys IPC; on any failure
// (hidsys unavailable, old firmware, no serial) it writes the index-based
// fallback `"switch-gamepad-<index>"`. Always NUL-terminates `out`.
void nx_gamepad_resolve_id(v8::Isolate *iso, unsigned index, char *out,
                           size_t out_len);

// Non-blocking check of the OS unique-pad connection event. When it has
// signaled (a controller connected/disconnected), clears the entire id cache
// and returns true. Returns false (and does nothing) otherwise. Exposed to JS
// as `$.gamepadConnectionChanged()` and called once per frame to drive the
// `gamepadconnected` / `gamepaddisconnected` sweep.
void nx_init_hidsys(v8::Isolate *iso, v8::Local<v8::Object> init_obj);
