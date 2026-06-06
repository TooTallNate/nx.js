#include "ui.h"

#include <dirent.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <switch.h>

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

static void header(void) {
	printf("nx.js slim launcher\n");
	printf("-------------------\n\n");
}

void nx_fail(const char *fmt, ...) {
	consoleInit(NULL);
	header();
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	wait_for_plus_and_exit();
}

void nx_fail_no_runtime(const nx_resolve_t *r) {
	consoleInit(NULL);
	header();
	printf("No compatible nx.js runtime found.\n\n");
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
	wait_for_plus_and_exit();
}
