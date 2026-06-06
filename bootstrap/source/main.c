// nx.js slim bootstrap launcher.
//
// A tiny homebrew NRO that does NOT embed the nx.js runtime. Instead it
// chainloads a SHARED runtime NRO installed under `sdmc:/nx.js/` and hands it
// this NRO as the app to run (argv[1]). The runtime then mounts THIS NRO's
// RomFS (which holds the app's `main.js` + assets) as `romfs:` and runs
// `romfs:/main.js` — see `mount_nro_romfs()` / the argv[1] entrypoint path in
// the runtime's `source/main.cc`.
//
// Runtime selection:
//   - This NRO's RomFS may contain `nxjs.ini` with a `[runtime] version`
//     semver specifier (e.g. "^1", ">=1.2", "1.0.0-beta.1"). Default: "^<major>"
//     baked at build time (NXJS_RUNTIME_DEFAULT).
//   - We enumerate `sdmc:/nx.js/nxjs-v<full-version>.nro`, parse each filename's
//     version, and pick the HIGHEST installed version that satisfies the
//     specifier.
//   - Found -> envSetNextLoad(that runtime, "\"<runtime>\" \"<self>\"") + exit.
//   - Not found / unsupported env -> on-screen error + wait for + to exit.
//
// This binary stays tiny: libnx + a vendored INI + semver parser, no V8/Skia.

#include <dirent.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <switch.h>

#include "match.h"

#define INI_IMPLEMENTATION
#include "vendor/ini.h"

#ifndef NXJS_RUNTIME_DEFAULT
#define NXJS_RUNTIME_DEFAULT "*"
#endif

#define RUNTIME_DIR "sdmc:/nx.js"
#define RUNTIME_PREFIX "nxjs-v"
#define RUNTIME_SUFFIX ".nro"

// --- INI: pull [runtime] version --------------------------------------------

typedef struct {
	char version[128];
	bool have_version;
} launcher_cfg_t;

static int ini_cb(void *user, const char *section, const char *name,
                  const char *value) {
	launcher_cfg_t *cfg = (launcher_cfg_t *)user;
	if (!section || !name || !value)
		return 1;
	if (strcasecmp(section, "runtime") == 0 &&
	    strcasecmp(name, "version") == 0) {
		snprintf(cfg->version, sizeof(cfg->version), "%s", value);
		cfg->have_version = true;
	}
	return 1; // keep parsing; we ignore everything else (the runtime reads it)
}

// --- Specifier parsing ------------------------------------------------------
//
// Specifier parsing + matching lives in match.c (nx_parse_spec /
// nx_version_satisfies / nx_version_compare) so it can be unit-tested on the
// host without libnx.

// --- main -------------------------------------------------------------------

static PadState g_pad;

static void wait_for_plus_and_exit(void) {
	padConfigureInput(1, HidNpadStyleSet_NpadStandard);
	padInitializeDefault(&g_pad);
	printf("\nPress + to exit.\n");
	consoleUpdate(NULL);
	while (appletMainLoop()) {
		padUpdate(&g_pad);
		if (padGetButtonsDown(&g_pad) & HidNpadButton_Plus)
			break;
		consoleUpdate(NULL);
	}
	consoleExit(NULL);
}

static void fail(const char *fmt, ...) {
	consoleInit(NULL);
	printf("nx.js slim launcher\n");
	printf("-------------------\n\n");
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	wait_for_plus_and_exit();
}

int main(int argc, char *argv[]) {
	// We must have been launched via hbloader (so we can chainload) and know
	// our own NRO path (argv[0]) to hand to the runtime.
	if (!envHasNextLoad()) {
		fail("This environment can't chainload the nx.js runtime.\n"
		     "Launch this app from the homebrew menu (hbmenu).\n");
		return 0;
	}
	const char *self = (argc > 0 && argv[0] && argv[0][0]) ? argv[0] : NULL;
	if (!self) {
		fail("Could not determine this app's own path (argv[0] missing).\n");
		return 0;
	}

	// Read the desired runtime version specifier from our own RomFS.
	launcher_cfg_t cfg = {0};
	snprintf(cfg.version, sizeof(cfg.version), "%s", NXJS_RUNTIME_DEFAULT);
	if (R_SUCCEEDED(romfsMountSelf("romfs"))) {
		ini_parse("romfs:/nxjs.ini", ini_cb, &cfg);
		romfsUnmount("romfs");
	}

	// Validate the specifier up front (gives a clear error before scanning).
	{
		char op[4] = {0};
		char ver[128] = {0};
		if (!nx_parse_spec(cfg.version, op, sizeof(op), ver, sizeof(ver))) {
			fail("Invalid [runtime] version specifier: \"%s\"\n", cfg.version);
			return 0;
		}
	}

	// Scan sdmc:/nx.js for nxjs-v<version>.nro and pick the highest match.
	char best_path[FS_MAX_PATH] = {0};
	char best_ver_str[128] = {0};
	bool have_best = false;

	DIR *dir = opendir(RUNTIME_DIR);
	if (dir) {
		struct dirent *ent;
		size_t plen = strlen(RUNTIME_PREFIX);
		size_t slen = strlen(RUNTIME_SUFFIX);
		while ((ent = readdir(dir)) != NULL) {
			const char *fn = ent->d_name;
			size_t fnlen = strlen(fn);
			if (fnlen <= plen + slen)
				continue;
			if (strncasecmp(fn, RUNTIME_PREFIX, plen) != 0)
				continue;
			if (strcasecmp(fn + fnlen - slen, RUNTIME_SUFFIX) != 0)
				continue;
			// Extract the version substring between prefix and suffix.
			char vbuf[128];
			size_t vlen = fnlen - plen - slen;
			if (vlen == 0 || vlen >= sizeof(vbuf))
				continue;
			memcpy(vbuf, fn + plen, vlen);
			vbuf[vlen] = '\0';
			if (!nx_version_satisfies(vbuf, cfg.version))
				continue;
			if (!have_best || nx_version_compare(vbuf, best_ver_str) > 0) {
				have_best = true;
				snprintf(best_ver_str, sizeof(best_ver_str), "%s", vbuf);
				snprintf(best_path, sizeof(best_path), "%s/%s", RUNTIME_DIR, fn);
			}
		}
		closedir(dir);
	}

	if (!have_best) {
		consoleInit(NULL);
		printf("nx.js slim launcher\n");
		printf("-------------------\n\n");
		printf("No compatible nx.js runtime found.\n\n");
		printf("Required: %s\n", cfg.version);
		printf("Looked in: %s/%s*%s\n\n", RUNTIME_DIR, RUNTIME_PREFIX,
		       RUNTIME_SUFFIX);
		// List what IS installed, to help the user.
		DIR *d2 = opendir(RUNTIME_DIR);
		if (d2) {
			struct dirent *e2;
			bool any = false;
			printf("Installed runtimes:\n");
			while ((e2 = readdir(d2)) != NULL) {
				if (strncasecmp(e2->d_name, RUNTIME_PREFIX,
				                strlen(RUNTIME_PREFIX)) == 0) {
					printf("  - %s\n", e2->d_name);
					any = true;
				}
			}
			if (!any)
				printf("  (none)\n");
			closedir(d2);
		} else {
			printf("(%s does not exist)\n", RUNTIME_DIR);
		}
		printf("\nInstall a runtime NRO to %s/ and try again.\n", RUNTIME_DIR);
		wait_for_plus_and_exit();
		return 0;
	}

	// Chainload: hand the runtime our own NRO as argv[1]. The runtime mounts
	// our RomFS as `romfs:` and runs `romfs:/main.js`.
	char args[FS_MAX_PATH * 2 + 8];
	snprintf(args, sizeof(args), "\"%s\" \"%s\"", best_path, self);
	Result rc = envSetNextLoad(best_path, args);
	if (R_FAILED(rc)) {
		fail("Failed to queue the nx.js runtime for launch (0x%x).\n"
		     "Runtime: %s\n",
		     rc, best_path);
		return 0;
	}
	// Returning hands control back to hbloader, which loads the runtime NRO.
	return 0;
}
