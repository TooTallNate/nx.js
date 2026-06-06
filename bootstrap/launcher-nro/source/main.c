// nx.js slim bootstrap launcher (NRO flavor).
//
// A tiny homebrew NRO that does NOT embed the nx.js runtime. It chainloads a
// SHARED runtime NRO installed under `sdmc:/nx.js/` and hands it THIS NRO as the
// app to run (argv[1]). The runtime then mounts this NRO's RomFS (the app's
// `main.js` + assets) as `romfs:` and runs `romfs:/main.js` — see the argv[1]
// entrypoint path in the runtime's `source/main.cc`.
//
// The shared runtime selection (read `[runtime] version` from our own RomFS,
// scan `sdmc:/nx.js/nxjs-v<version>.nro`, semver-match the highest) lives in the
// shared `resolve.c`. The ONLY NRO-specific part is the launch: envSetNextLoad +
// exit, which works because we were launched by hbloader.
//
// This binary stays tiny: libnx + a vendored INI + semver parser, no V8/Skia.

#include <stdbool.h>
#include <stdio.h>
#include <switch.h>

#include "resolve.h"
#include "ui.h"

int main(int argc, char *argv[]) {
	// We must have been launched via hbloader (so we can chainload) and know
	// our own NRO path (argv[0]) to hand to the runtime.
	if (!envHasNextLoad()) {
		nx_fail("This environment can't chainload the nx.js runtime.\n"
		        "Launch this app from the homebrew menu (hbmenu).\n");
		return 0;
	}
	const char *self = (argc > 0 && argv[0] && argv[0][0]) ? argv[0] : NULL;
	if (!self) {
		nx_fail("Could not determine this app's own path (argv[0] missing).\n");
		return 0;
	}

	// Read the desired runtime version requirement from our own RomFS.
	nx_resolve_t r = {0};
	if (R_SUCCEEDED(romfsMountSelf("romfs"))) {
		bool ok = nx_read_runtime_requirement(&r);
		romfsUnmount("romfs");
		if (!ok) {
			nx_fail("Invalid [runtime] version specifier: \"%s\"\n", r.version);
			return 0;
		}
	} else {
		// No RomFS? Fall back to the baked default requirement.
		nx_read_runtime_requirement(&r);
	}

	if (!nx_resolve_runtime(&r)) {
		nx_fail_no_runtime(&r);
		return 0;
	}

	// Chainload: hand the runtime our own NRO as argv[1]. The runtime mounts
	// our RomFS as `romfs:` and runs `romfs:/main.js`.
	char args[FS_MAX_PATH * 2 + 8];
	snprintf(args, sizeof(args), "\"%s\" \"%s\"", r.runtime_path, self);
	Result rc = envSetNextLoad(r.runtime_path, args);
	if (R_FAILED(rc)) {
		nx_fail("Failed to queue the nx.js runtime for launch (0x%x).\n"
		        "Runtime: %s\n",
		        rc, r.runtime_path);
		return 0;
	}
	// Returning hands control back to hbloader, which loads the runtime NRO.
	return 0;
}
