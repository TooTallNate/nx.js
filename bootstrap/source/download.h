#pragma once

// Shared nx.js bootstrap logic: download a compatible shared runtime NRO from
// the nx.js GitHub releases when none is installed on the SD card. Used by BOTH
// launcher flavors from their "no runtime found" path. Pure libnx (BSD sockets
// + the system `ssl` service for HTTPS; no curl/mbedtls), so it links into the
// network-free launchers without extra dependencies.
//
// Flow (see download.c):
//   1. Bring up networking (nifm) + the ssl/socket services.
//   2. GET https://api.github.com/repos/<repo>/releases, scrape the `tag_name`
//      values (each "v<version>"), and pick the HIGHEST release whose version
//      satisfies the app's requirement (reusing the semver matcher).
//   3. Download https://github.com/<repo>/releases/download/v<ver>/nxjs.nro
//      (following GitHub's redirect to the CDN), streaming progress on-screen.
//   4. Save it as sdmc:/nx.js/nxjs-v<ver>.nro (the SD naming convention) via a
//      temp file + rename, so a partial download can't masquerade as installed.

#include <stdbool.h>
#include <switch.h>

#include "resolve.h"

// The GitHub repository that publishes the runtime releases + asset filename.
#define NXJS_GH_REPO "TooTallNate/nx.js"
#define NXJS_RELEASE_ASSET "nxjs.nro"

// Attempt to download a runtime satisfying `r->version` into NXJS_RUNTIME_DIR.
// Renders progress/status to the (already-initialized) console. On success,
// returns true and fills `r->runtime_path` / `r->runtime_version` with the
// freshly-downloaded runtime (so the caller can launch it without re-scanning).
// On any failure (no network, no satisfying release, HTTP/TLS/IO error) prints
// the reason and returns false; the caller should fall back to its error UI.
//
// Requires the SD card to be mounted (fsdevMountSdmc) and a console up.
bool nx_download_runtime(nx_resolve_t *r);
