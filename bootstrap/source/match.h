#pragma once

// Pure (no-libnx) runtime-version matching helpers for the slim bootstrap
// launcher. Split out from main.c so they can be unit-tested on the host
// (see bootstrap/test/), where libnx isn't available.

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Normalize a version-ish string into X.Y.Z[-pre][+meta] by padding missing
// minor/patch with ".0". Writes into `out` (size `out_sz`).
void nx_normalize_version(const char *in, char *out, size_t out_sz);

// Parse a semver specifier (e.g. "^1", ">=1.2", "1.0.0-beta.1", "*", "1") into
// an operator string (for semver_satisfies, e.g. "^", ">=", "=") and a
// normalized X.Y.Z version string. Returns true on success.
bool nx_parse_spec(const char *spec, char *op, size_t op_sz, char *ver,
                   size_t ver_sz);

// Given a runtime version string `cand` (e.g. "1.2.0", "1.0.0-beta.1") and a
// specifier `spec`, return true if `cand` satisfies `spec`.
bool nx_version_satisfies(const char *cand, const char *spec);

// Compare two version strings: -1 if a<b, 0 if equal, 1 if a>b. Invalid
// versions sort lowest.
int nx_version_compare(const char *a, const char *b);

#ifdef __cplusplus
}
#endif
