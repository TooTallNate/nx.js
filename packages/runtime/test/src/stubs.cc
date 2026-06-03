/**
 * Host stubs for nxjs-test (V8 + libuv era).
 *
 * No-op nx_init_* for the Switch-only modules that have no host
 * implementation. The portable modules (canvas, crypto, dns, compression,
 * url, web, fs, image, font, tcp, tls, udp, wasm, window, dommatrix, error,
 * util, async) are compiled from source/*.cc against the libnx stubs in
 * compat/. memory.cc is Switch-specific (svc memory introspection), so it's
 * stubbed here too.
 */
#include "types.h"
#include <v8.h>

#define STUB_MOD(name)                                                         \
	void nx_init_##name(v8::Isolate *iso, v8::Local<v8::Object> init_obj) {    \
		(void)iso;                                                             \
		(void)init_obj;                                                        \
	}

STUB_MOD(account)
STUB_MOD(album)
STUB_MOD(applet)
STUB_MOD(audio)
STUB_MOD(battery)
STUB_MOD(fsdev)
STUB_MOD(gamepad)
STUB_MOD(irs)
STUB_MOD(memory)
STUB_MOD(nifm)
STUB_MOD(ns)
STUB_MOD(service)
STUB_MOD(swkbd)
STUB_MOD(web)  // WebView applet: libnx-only, no host impl + no fixture
#undef STUB_MOD
