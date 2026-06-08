#include "match.h"

#include <stdio.h>
#include <string.h>

#include "vendor/semver.h"

void nx_normalize_version(const char *in, char *out, size_t out_sz) {
	// Count the dotted numeric components before any '-'/'+'.
	int dots = 0;
	for (const char *q = in; *q && *q != '-' && *q != '+'; q++) {
		if (*q == '.')
			dots++;
	}
	if (dots >= 2) {
		snprintf(out, out_sz, "%s", in);
		return;
	}
	// Find where the core (major[.minor]) ends (start of -pre / +meta / NUL).
	const char *rest = in;
	while (*rest && *rest != '-' && *rest != '+')
		rest++;
	int core_len = (int)(rest - in);
	if (dots == 0)
		snprintf(out, out_sz, "%.*s.0.0%s", core_len, in, rest);
	else // dots == 1
		snprintf(out, out_sz, "%.*s.0%s", core_len, in, rest);
}

bool nx_parse_spec(const char *spec, char *op, size_t op_sz, char *ver,
                   size_t ver_sz) {
	while (*spec == ' ' || *spec == '\t' || *spec == 'v' || *spec == 'V')
		spec++;
	if (*spec == '*' || *spec == '\0') {
		// Wildcard: match anything. Represent as ">= 0.0.0".
		snprintf(op, op_sz, ">=");
		snprintf(ver, ver_sz, "0.0.0");
		return true;
	}
	// Leading operator?
	if (spec[0] == '^' || spec[0] == '~' || spec[0] == '=') {
		snprintf(op, op_sz, "%c", spec[0]);
		spec++;
	} else if (spec[0] == '>' || spec[0] == '<') {
		if (spec[1] == '=') {
			snprintf(op, op_sz, "%c=", spec[0]);
			spec += 2;
		} else {
			snprintf(op, op_sz, "%c", spec[0]);
			spec += 1;
		}
	} else {
		// Bare version -> caret range.
		snprintf(op, op_sz, "^");
	}
	while (*spec == ' ' || *spec == '\t' || *spec == 'v' || *spec == 'V')
		spec++;
	// Reject absurdly long inputs so normalization (which may append ".0.0")
	// can't overflow/truncate the caller's buffer.
	if (strlen(spec) >= 96)
		return false;
	nx_normalize_version(spec, ver, ver_sz);
	return ver[0] != '\0';
}

bool nx_version_satisfies(const char *cand, const char *spec) {
	char op[4] = {0};
	char want_str[128] = {0};
	if (!nx_parse_spec(spec, op, sizeof(op), want_str, sizeof(want_str)))
		return false;
	// NOTE: semver_parse() can allocate metadata/prerelease even when it
	// returns an error (it slices +/- before parsing the numeric version), so
	// always semver_free() — both structs are zero-initialized, and freeing a
	// zeroed semver_t is a no-op.
	semver_t want = {0};
	semver_t c = {0};
	int pw = semver_parse(want_str, &want);
	int pc = semver_parse(cand, &c);
	bool ok = (pw == 0 && pc == 0) && semver_satisfies(c, want, op) != 0;
	// Prerelease floor guard. The vendored semver's caret/tilde/range checks
	// compare only MAJOR.MINOR.PATCH and ignore the prerelease component, so a
	// spec with a prerelease lower bound (e.g. "^1.0.0-beta.2", which @nx.js/nro
	// and @nx.js/nsp now bake) would wrongly accept an OLDER prerelease of the
	// same x.y.z (e.g. "1.0.0-beta.1"). For floor operators, when the candidate
	// shares want's x.y.z and want carries a prerelease, additionally require
	// the candidate to be >= want by full precedence (which orders prereleases).
	if (ok && want.prerelease && want.prerelease[0] &&
	    (op[0] == '^' || op[0] == '~' || op[0] == '=' ||
	     (op[0] == '>' && op[1] == '=')) &&
	    c.major == want.major && c.minor == want.minor &&
	    c.patch == want.patch) {
		if (semver_compare(c, want) < 0)
			ok = false;
	}
	semver_free(&c);
	semver_free(&want);
	return ok;
}

int nx_version_compare(const char *a, const char *b) {
	semver_t va = {0}, vb = {0};
	int pa = semver_parse(a, &va);
	int pb = semver_parse(b, &vb);
	int r;
	if (pa != 0 && pb != 0)
		r = 0;
	else if (pa != 0)
		r = -1;
	else if (pb != 0)
		r = 1;
	else
		r = semver_compare(va, vb);
	// semver_parse() may allocate metadata/prerelease even on an error return,
	// so always free (both structs are zero-initialized; freeing is a no-op).
	semver_free(&va);
	semver_free(&vb);
	return r;
}
