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
	semver_t want = {0};
	if (semver_parse(want_str, &want) != 0)
		return false;
	semver_t c = {0};
	if (semver_parse(cand, &c) != 0) {
		semver_free(&want);
		return false;
	}
	bool ok = semver_satisfies(c, want, op) != 0;
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
	if (pa == 0)
		semver_free(&va);
	if (pb == 0)
		semver_free(&vb);
	return r;
}
