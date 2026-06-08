#include "ui.h"

#include <dirent.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <switch.h>

#include "download.h"

static PadState g_pad;

// Launcher-provided exit hook. The default (used by the NRO launcher, which is
// launched by hbloader and exits normally) just tears the console down and
// returns to the caller's crt0. The NSP forwarder overrides this with a strong
// definition that performs the proper applet self-exit handshake (it links
// -Wl,-wrap,exit and runs as forwarded homebrew, so a normal exit() can't be
// used). Does not return for the forwarder; returns for the NRO launcher.
__attribute__((weak)) void nx_ui_exit(void) {
	consoleExit(NULL);
}

static void wait_for_plus_and_exit(void) {
	padConfigureInput(1, HidNpadStyleSet_NpadStandard);
	padInitializeDefault(&g_pad);
	printf("\n" NX_C_GRAY "Press " NX_C_RESET NX_C_BOLD "+" NX_C_RESET NX_C_GRAY
	                      " to exit." NX_C_RESET "\n");
	consoleUpdate(NULL);
	while (appletMainLoop()) {
		padUpdate(&g_pad);
		if (padGetButtonsDown(&g_pad) & HidNpadButton_Plus)
			break;
		consoleUpdate(NULL);
	}
	nx_ui_exit();
}

static void header(void) {
	printf("\n  " NX_C_BOLD NX_C_CYAN "nx.js" NX_C_RESET " launcher\n");
	printf("  --------------\n\n");
}

void nx_failv(const char *fmt, va_list ap) {
	consoleInit(NULL);
	header();
	vprintf(fmt, ap);
	wait_for_plus_and_exit();
}

void nx_fail(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	nx_failv(fmt, ap);
	va_end(ap);
}

// Print the "what's required / where we looked / what's installed / how to
// install manually" block. Assumes the console is initialized + header printed.
static void print_no_runtime_details(const nx_resolve_t *r) {
	printf("  " NX_C_GRAY "required " NX_C_RESET NX_C_BOLD "%s" NX_C_RESET "\n",
	       r->version);
	printf("  " NX_C_GRAY "looked in " NX_C_RESET "%s/%s*%s" "\n\n",
	       NXJS_RUNTIME_DIR, NXJS_RUNTIME_PREFIX, NXJS_RUNTIME_SUFFIX);
	// List what IS installed, to help the user.
	DIR *d = opendir(NXJS_RUNTIME_DIR);
	if (d) {
		struct dirent *e;
		bool any = false;
		printf("  " NX_C_GRAY "installed runtimes:" NX_C_RESET "\n");
		while ((e = readdir(d)) != NULL) {
			if (strncasecmp(e->d_name, NXJS_RUNTIME_PREFIX,
			                strlen(NXJS_RUNTIME_PREFIX)) == 0) {
				printf("    - %s\n", e->d_name);
				any = true;
			}
		}
		if (!any)
			printf("    " NX_C_DIM "(none)" NX_C_RESET "\n");
		closedir(d);
	} else {
		printf("  " NX_C_DIM "(%s does not exist)" NX_C_RESET "\n",
		       NXJS_RUNTIME_DIR);
	}
	printf("\n  To install it manually, download the runtime NRO from\n");
	printf("  " NX_C_CYAN "https://github.com/" NXJS_GH_REPO "/releases"
	       NX_C_RESET "\n");
	printf("  and place it in " NX_C_BOLD "%s/" NX_C_RESET "\n",
	       NXJS_RUNTIME_DIR);
}

void nx_fail_no_runtime(const nx_resolve_t *r) {
	consoleInit(NULL);
	header();
	printf("  " NX_C_YELLOW "No compatible nx.js runtime is installed." NX_C_RESET
	       "\n\n");
	print_no_runtime_details(r);
	wait_for_plus_and_exit();
}

bool nx_resolve_or_download(nx_resolve_t *r) {
	consoleInit(NULL);
	header();
	printf("  " NX_C_YELLOW "No compatible nx.js runtime is installed." NX_C_RESET
	       "\n");
	// Emphasize this is a one-time setup so the wait doesn't feel like an error.
	printf("  " NX_C_DIM "This one-time setup downloads the shared runtime; it "
	       "is\n  reused by every nx.js app afterwards." NX_C_RESET "\n\n");
	consoleUpdate(NULL);

	// Auto-download a compatible runtime from the GitHub releases. Renders its
	// own colored progress/status lines to the console.
	if (nx_download_runtime(r)) {
		printf("\n  " NX_C_GREEN "Ready - launching the app..." NX_C_RESET "\n");
		consoleUpdate(NULL);
		return true; // r now points at the downloaded runtime
	}

	// Download unavailable/failed — nx_download_runtime already printed the
	// specific reason (offline, no satisfying release, etc.). Fall back to the
	// manual-install instructions.
	printf("\n");
	print_no_runtime_details(r);
	wait_for_plus_and_exit();
	return false; // unreachable (wait_for_plus_and_exit -> nx_ui_exit)
}
