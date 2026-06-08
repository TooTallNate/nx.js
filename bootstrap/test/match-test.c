// Host unit test for the slim launcher's runtime-version matching logic
// (bootstrap/source/match.c). Compiles + runs on the host (no libnx), so the
// semver specifier handling can be verified without Switch hardware.
//
// Build & run:
//   cc -I../source -o /tmp/match-test match-test.c ../source/match.c \
//      ../source/vendor/semver.c && /tmp/match-test
//
// (See bootstrap/test/run.sh.)

#include <stdio.h>
#include <string.h>

#include "match.h"

static int failures = 0;
static int checks = 0;

static void expect_satisfies(const char *cand, const char *spec, bool want) {
	checks++;
	bool got = nx_version_satisfies(cand, spec);
	if (got != want) {
		failures++;
		printf("FAIL: nx_version_satisfies(\"%s\", \"%s\") = %d, expected %d\n",
		       cand, spec, got, want);
	}
}

static void expect_cmp(const char *a, const char *b, int want_sign) {
	checks++;
	int r = nx_version_compare(a, b);
	int sign = (r > 0) - (r < 0);
	if (sign != want_sign) {
		failures++;
		printf("FAIL: nx_version_compare(\"%s\", \"%s\") sign = %d, expected %d\n",
		       a, b, sign, want_sign);
	}
}

int main(void) {
	// --- Bare specifier => caret-on-major semantics ---
	expect_satisfies("1.0.0", "1", true);
	expect_satisfies("1.5.9", "1", true);
	expect_satisfies("2.0.0", "1", false);
	expect_satisfies("0.9.0", "1", false);

	// --- Caret ---
	expect_satisfies("1.2.3", "^1.2.0", true);
	expect_satisfies("1.9.9", "^1.2.0", true);
	expect_satisfies("2.0.0", "^1.2.0", false);
	expect_satisfies("1.1.0", "^1.2.0", false); // below the floor

	// --- Tilde (patch-level within minor) ---
	expect_satisfies("1.2.9", "~1.2.0", true);
	expect_satisfies("1.3.0", "~1.2.0", false);

	// --- Comparators ---
	expect_satisfies("1.2.0", ">=1.2.0", true);
	expect_satisfies("1.5.0", ">=1.2.0", true);
	expect_satisfies("1.1.0", ">=1.2.0", false);
	expect_satisfies("2.0.0", ">1", true);
	expect_satisfies("0.9.0", "<1", true);
	expect_satisfies("1.0.0", "<1", false);
	expect_satisfies("1.0.0", "=1.0.0", true);
	expect_satisfies("1.0.1", "=1.0.0", false);

	// --- Wildcard ---
	expect_satisfies("0.0.1", "*", true);
	expect_satisfies("99.5.2", "*", true);

	// --- Partial versions in specifier (minor) ---
	expect_satisfies("1.2.5", "1.2", true);  // bare 1.2 => ^1.2.0
	// Caret allows minor/patch bumps within the same major, so 1.3.0 DOES
	// satisfy ^1.2.0 (== >=1.2.0 <2.0.0).
	expect_satisfies("1.3.0", "1.2", true);
	expect_satisfies("2.0.0", "1.2", false); // major bump is out of range

	// --- Prerelease ---
	// NOTE: the vendored semver.c does NOT implement node-semver's rule that a
	// prerelease is excluded from a non-prerelease range. It compares purely by
	// precedence, so a bare "1" (=> ^1.0.0) DOES match a "1.0.0-beta.N" build.
	// During the v1 beta this is intentionally convenient: an app requesting
	// "^1" can run on the only existing (prerelease) runtime.
	expect_satisfies("1.0.0-beta.1", "1", true);
	// Explicit prerelease ranges also work by precedence.
	expect_satisfies("1.0.0-beta.2", ">=1.0.0-beta.1", true);
	expect_satisfies("1.0.0-beta.1", ">=1.0.0-beta.1", true);
	expect_satisfies("1.0.0-beta.1", ">=1.0.0-beta.2", false);

	// --- Caret on a FULL prerelease version (what @nx.js/nro|nsp now bake,
	// e.g. "^1.0.0-beta.2": "at least the version packaged with, or newer").
	// By precedence, ^1.0.0-beta.2 == >=1.0.0-beta.2 <2.0.0.
	expect_satisfies("1.0.0-beta.2", "^1.0.0-beta.2", true); // exact floor
	expect_satisfies("1.0.0-beta.3", "^1.0.0-beta.2", true); // newer prerelease
	expect_satisfies("1.0.0", "^1.0.0-beta.2", true);        // the release
	expect_satisfies("1.2.0", "^1.0.0-beta.2", true);        // newer minor
	expect_satisfies("1.0.0-beta.1", "^1.0.0-beta.2", false); // OLDER -> rejected
	expect_satisfies("2.0.0", "^1.0.0-beta.2", false);        // major bump out

	// --- Comparison / "pick highest" ordering ---
	expect_cmp("1.2.0", "1.10.0", -1);  // numeric, not lexical
	expect_cmp("1.10.0", "1.2.0", 1);
	expect_cmp("1.0.0", "1.0.0", 0);
	expect_cmp("2.0.0", "1.9.9", 1);
	// Prerelease sorts below its release.
	expect_cmp("1.0.0-beta.1", "1.0.0", -1);
	expect_cmp("1.0.0-beta.2", "1.0.0-beta.1", 1);
	// Invalid versions sort lowest.
	expect_cmp("not-a-version", "1.0.0", -1);

	printf("%d checks, %d failures\n", checks, failures);
	return failures == 0 ? 0 : 1;
}
