#pragma once

// Shared on-screen error UI for the nx.js bootstrap launchers. Both flavors
// show the same messages (init the console, print, wait for + to exit) so a
// missing/incompatible runtime is a clear message, never a crash.

#include <stdarg.h>

#include "resolve.h"

// ANSI SGR escapes for the on-screen console. libnx's PrintConsole supports
// only the BASIC set — the 30-37 foreground colors, bold (1) and reset (0).
// The bright 90-97 colors and dim (2) are NOT supported (they print garbage),
// and the framebuffer font has no box-drawing/checkmark glyphs, so we use ASCII
// only. "Dim"/"gray" therefore map to plain (no color) on purpose.
#define NX_C_RESET "\x1b[0m"
#define NX_C_BOLD "\x1b[1m"
#define NX_C_DIM ""
#define NX_C_RED "\x1b[31m"
#define NX_C_GREEN "\x1b[32m"
#define NX_C_YELLOW "\x1b[33m"
#define NX_C_BLUE "\x1b[34m"
#define NX_C_CYAN "\x1b[36m"
#define NX_C_GRAY ""

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
// This calls consoleInit() itself (idempotent), so the console need not be
// initialized beforehand. The NSP forwarder must still call nx_ui_bringup
// first, though, to bring up the display/input services the console needs.
bool nx_resolve_or_download(nx_resolve_t *r);

// Exit hook invoked after the user presses + on any nx_fail* screen. Weakly
// defined in ui.c (consoleExit + return, for the hbloader-launched NRO
// launcher). A launcher whose process can't exit normally (the NSP forwarder)
// provides a strong override that performs the proper applet self-exit
// handshake; in that case nx_ui_exit() does not return.
void nx_ui_exit(void);
