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
	printf("\nPress + to exit.\n");
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
	printf("nx.js slim launcher\n");
	printf("-------------------\n\n");
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

// Print the "what's required / where we looked / what's installed" block.
// Assumes the console is already initialized + the header was printed.
static void print_no_runtime_details(const nx_resolve_t *r) {
	printf("Required: %s\n", r->version);
	printf("Looked in: %s/%s*%s\n\n", NXJS_RUNTIME_DIR, NXJS_RUNTIME_PREFIX,
	       NXJS_RUNTIME_SUFFIX);
	// List what IS installed, to help the user.
	DIR *d = opendir(NXJS_RUNTIME_DIR);
	if (d) {
		struct dirent *e;
		bool any = false;
		printf("Installed runtimes:\n");
		while ((e = readdir(d)) != NULL) {
			if (strncasecmp(e->d_name, NXJS_RUNTIME_PREFIX,
			                strlen(NXJS_RUNTIME_PREFIX)) == 0) {
				printf("  - %s\n", e->d_name);
				any = true;
			}
		}
		if (!any)
			printf("  (none)\n");
		closedir(d);
	} else {
		printf("(%s does not exist)\n", NXJS_RUNTIME_DIR);
	}
	printf("\nInstall a runtime NRO to %s/ and try again.\n", NXJS_RUNTIME_DIR);
}

void nx_fail_no_runtime(const nx_resolve_t *r) {
	consoleInit(NULL);
	header();
	printf("No compatible nx.js runtime found.\n\n");
	print_no_runtime_details(r);
	wait_for_plus_and_exit();
}

bool nx_resolve_or_download(nx_resolve_t *r) {
	consoleInit(NULL);
	header();
	printf("No compatible nx.js runtime found.\n\n");
	consoleUpdate(NULL);

	// Auto-download a compatible runtime from the GitHub releases. Renders its
	// own progress/status lines to the console.
	if (nx_download_runtime(r)) {
		printf("\nReady. Launching...\n");
		consoleUpdate(NULL);
		return true; // r now points at the downloaded runtime
	}

	// Download unavailable/failed — fall back to the manual-install screen.
	printf("\nCould not download the runtime automatically.\n\n");
	print_no_runtime_details(r);
	wait_for_plus_and_exit();
	return false; // unreachable (wait_for_plus_and_exit -> nx_ui_exit)
}
