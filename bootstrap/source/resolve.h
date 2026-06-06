#pragma once

// Shared nx.js bootstrap logic: locate the best shared runtime NRO on the SD
// card given an app's `[runtime] version` requirement. Used by BOTH launcher
// flavors:
//   - launcher-nro:  envSetNextLoad()s the resolved runtime (hbloader chainload)
//   - launcher-nsp:  a forwarder hbloader maps + jumps into the resolved runtime
// Only that final "launch" step differs; everything up to picking the runtime
// NRO is shared here. (A future "download the runtime if missing" step will
// also slot in here.)

#include <stdbool.h>
#include <switch.h>

// Default `[runtime] version` requirement, baked at build time (e.g. "^1").
#ifndef NXJS_RUNTIME_DEFAULT
#define NXJS_RUNTIME_DEFAULT "*"
#endif

// Where shared runtimes live, and the filename scheme: nxjs-v<version>.nro.
#define NXJS_RUNTIME_DIR "sdmc:/nx.js"
#define NXJS_RUNTIME_PREFIX "nxjs-v"
#define NXJS_RUNTIME_SUFFIX ".nro"

typedef struct {
	// The semver requirement read from `romfs:/nxjs.ini` [runtime] version
	// (or NXJS_RUNTIME_DEFAULT if absent).
	char version[128];
	// Filled in by nx_resolve_runtime on success:
	char runtime_path[FS_MAX_PATH]; // e.g. "sdmc:/nx.js/nxjs-v1.0.0.nro"
	char runtime_version[128];      // the matched version string
} nx_resolve_t;

// Read the `[runtime] version` requirement from `romfs:/nxjs.ini`. Assumes the
// caller has already mounted the app's RomFS as `romfs:`. Falls back to
// NXJS_RUNTIME_DEFAULT when the file/key is absent. Returns true if the
// requirement is a valid specifier (false ⇒ caller should error out).
bool nx_read_runtime_requirement(nx_resolve_t *out);

// Scan NXJS_RUNTIME_DIR for nxjs-v<version>.nro and pick the highest installed
// version satisfying `out->version`. On success fills runtime_path/runtime_version
// and returns true; returns false if none match.
bool nx_resolve_runtime(nx_resolve_t *out);
