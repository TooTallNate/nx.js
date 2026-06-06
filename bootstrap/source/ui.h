#pragma once

// Shared on-screen error UI for the nx.js bootstrap launchers. Both flavors
// show the same messages (init the console, print, wait for + to exit) so a
// missing/incompatible runtime is a clear message, never a crash.

#include "resolve.h"

// consoleInit + print "nx.js slim launcher" header + printf(fmt, ...) +
// wait for + and exit. Does not return.
void nx_fail(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// Show the standard "No compatible nx.js runtime found" screen (required
// version, where we looked, what's installed), then wait for + and exit.
// Does not return.
void nx_fail_no_runtime(const nx_resolve_t *r);
