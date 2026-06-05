#pragma once
#include "types.h"

// Defined by each runtime entrypoint (device main.cc / host test main.cc);
// switches the screen to the libnx text console (no-op on the host).
void nx_console_init(nx_context_t *nx_ctx);

// ---------------------------------------------------------------------------
// ES module loading (static `import` + filesystem `await import()`).
//
// Shared by both the device runtime (source/main.cc) and the host test binary
// (packages/runtime/test/src/main.cc) so the two never drift. Specifiers are
// resolved as URLs (via `ada`) against the importing module's URL and read
// synchronously with read_file()/fopen, so only mounted devoptab schemes
// (romfs:, sdmc:, nxjs:, file:) work; bare specifiers are rejected.
// ---------------------------------------------------------------------------

// Register the host module callbacks on the isolate:
//   - SetHostInitializeImportMetaObjectCallback (import.meta.url / .main)
//   - SetHostImportModuleDynamicallyCallback   (filesystem `import()`)
// Call once, after Isolate::New.
void nx_init_modules(v8::Isolate *iso);

// Compile + instantiate + evaluate `src` (length `len`) as the entrypoint ES
// module, recorded under URL `name` (its ScriptOrigin resource name and
// import.meta.url base). Resolves static + dynamic imports against the
// filesystem. Returns false on failure (the error is reported via
// nx_emit_error_event). Handles top-level await: a rejected async graph is
// surfaced via the error path rather than becoming a silent unhandled
// rejection.
bool nx_run_entry_module(v8::Isolate *iso, v8::Local<v8::Context> context,
                         const char *src, size_t len, const char *name);

// Release retained module handles (call before disposing the isolate).
void nx_modules_teardown();
