#pragma once
#include "types.h"

// The V8 inspector, exposed over the Chrome DevTools Protocol.
//
// `nx_init_inspector()` installs the `$.inspector*` bindings. Everything else
// here is called by the main loop, because the transport has to keep running
// when JavaScript is not — see `inspector.cc` for why.

void nx_init_inspector(v8::Isolate *iso, v8::Local<v8::Object> init_obj);

// Services the debugger socket. Cheap and non-blocking when no debugger is
// attached; call it once per turn of the event loop.
void nx_inspector_poll(v8::Isolate *iso);

// Closes the listening socket and any attached debugger.
void nx_inspector_shutdown();
