#include "resolve.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "match.h"

#define INI_IMPLEMENTATION
#include "vendor/ini.h"

static int ini_cb(void *user, const char *section, const char *name,
                  const char *value) {
	nx_resolve_t *out = (nx_resolve_t *)user;
	if (!section || !name || !value)
		return 1;
	if (strcasecmp(section, "runtime") == 0 &&
	    strcasecmp(name, "version") == 0) {
		snprintf(out->version, sizeof(out->version), "%s", value);
	}
	return 1; // keep parsing; ignore everything else (the runtime reads it)
}

bool nx_read_runtime_requirement(nx_resolve_t *out) {
	// Default requirement; overridden by romfs:/nxjs.ini [runtime] version.
	snprintf(out->version, sizeof(out->version), "%s", NXJS_RUNTIME_DEFAULT);
	ini_parse("romfs:/nxjs.ini", ini_cb, out);

	// Validate the specifier so callers can give a clear error before scanning.
	char op[4] = {0};
	char ver[128] = {0};
	return nx_parse_spec(out->version, op, sizeof(op), ver, sizeof(ver));
}

bool nx_resolve_runtime(nx_resolve_t *out) {
	bool have_best = false;
	out->runtime_path[0] = '\0';
	out->runtime_version[0] = '\0';

	DIR *dir = opendir(NXJS_RUNTIME_DIR);
	if (!dir)
		return false;

	struct dirent *ent;
	size_t plen = strlen(NXJS_RUNTIME_PREFIX);
	size_t slen = strlen(NXJS_RUNTIME_SUFFIX);
	while ((ent = readdir(dir)) != NULL) {
		const char *fn = ent->d_name;
		size_t fnlen = strlen(fn);
		if (fnlen <= plen + slen)
			continue;
		if (strncasecmp(fn, NXJS_RUNTIME_PREFIX, plen) != 0)
			continue;
		if (strcasecmp(fn + fnlen - slen, NXJS_RUNTIME_SUFFIX) != 0)
			continue;
		// Extract the version substring between prefix and suffix.
		char vbuf[128];
		size_t vlen = fnlen - plen - slen;
		if (vlen == 0 || vlen >= sizeof(vbuf))
			continue;
		memcpy(vbuf, fn + plen, vlen);
		vbuf[vlen] = '\0';
		if (!nx_version_satisfies(vbuf, out->version))
			continue;
		if (!have_best || nx_version_compare(vbuf, out->runtime_version) > 0) {
			have_best = true;
			snprintf(out->runtime_version, sizeof(out->runtime_version), "%s",
			         vbuf);
			snprintf(out->runtime_path, sizeof(out->runtime_path), "%s/%s",
			         NXJS_RUNTIME_DIR, fn);
		}
	}
	closedir(dir);
	return have_best;
}
