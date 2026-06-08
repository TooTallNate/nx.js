#pragma once

// Shared on-screen error UI for the nx.js bootstrap launchers. Both flavors
// show the same messages (init the console, print, wait for + to exit) so a
// missing/incompatible runtime is a clear message, never a crash.

#include <stdarg.h>

#include "resolve.h"

// consoleInit + print "nx.js slim launcher" header + printf(fmt, ...) +
// wait for + and exit. Does not return.
void nx_fail(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// va_list form of nx_fail() (so a launcher can forward its own varargs).
void nx_failv(const char *fmt, va_list ap);

// Show the standard "No compatible nx.js runtime found" screen (required
// version, where we looked, what's installed), then wait for + and exit.
// Does not return.
void nx_fail_no_runtime(const nx_resolve_t *r);

// "No runtime found" recovery: render the console, then automatically attempt
// to DOWNLOAD a compatible runtime from the nx.js GitHub releases (see
// download.c). On success, fills `r->runtime_path`/`runtime_version` and returns
// true so the caller can launch the freshly-downloaded runtime. On failure
// (no network, no satisfying release, IO error) it shows the standard
// "no runtime" install instructions and waits for + to exit — i.e. it behaves
// like nx_fail_no_runtime() and DOES NOT RETURN. So: a `true` return means a
// runtime is ready; the function not returning means the user exited.
//
// The console must already be initialized (the NRO launcher's nx_fail* do this;
// the NSP forwarder calls nx_ui_bringup first). `consoleInit` is idempotent.
bool nx_resolve_or_download(nx_resolve_t *r);

// Exit hook invoked after the user presses + on any nx_fail* screen. Weakly
// defined in ui.c (consoleExit + return, for the hbloader-launched NRO
// launcher). A launcher whose process can't exit normally (the NSP forwarder)
// provides a strong override that performs the proper applet self-exit
// handshake; in that case nx_ui_exit() does not return.
void nx_ui_exit(void);
