/**
 * nxjs-test — Unified host-platform test binary for nx.js
 *
 * This is a near-complete nx.js runtime that runs on the host system (x86_64
 * or aarch64) instead of the Nintendo Switch. It compiles the real nx.js C
 * source files against host-system libraries, with libnx functions stubbed
 * out or re-implemented where appropriate.
 *
 * Features:
 * - Full event loop running at ~60 FPS (like the real Switch app)
 * - Real thread pool for async operations (image decode, crypto, etc.)
 * - Real networking (TCP, UDP, DNS, TLS via poll system)
 * - Full runtime.js loaded at startup
 * - All portable native modules (canvas, crypto, compression, URL, WASM, etc.)
 *
 * Usage: nxjs-test <runtime.js> <script.js> [args...]
 */

#include <errno.h>
#include <mbedtls/psa_util.h>
#include <mbedtls/x509_crt.h>
#include <psa/crypto.h>
#include <quickjs.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Include all module headers — compat/ directory provides stubs for
// <switch.h>, <wasm3.h>, <mbedtls/*.h> that source/types.h pulls in.
#include "account.h"
#include "album.h"
#include "applet.h"
#include "async.h"
#include "audio.h"
#include "battery.h"
#include "canvas.h"
#include "compression.h"
#include "crypto.h"
#include "dns.h"
#include "dommatrix.h"
#include "error.h"
#include "font.h"
#include "fs.h"
#include "fsdev.h"
#include "gamepad.h"
#include "image.h"
#include "irs.h"
#include "nifm.h"
#include "ns.h"
#include "poll.h"
#include "service.h"
#include "software-keyboard.h"
#include "tcp.h"
#include "tls.h"
#include "types.h"
#include "udp.h"
#include "uint8array.h"
#include "url.h"
#include "util.h"
#include "wasm.h"
#include "web.h"
#include "window.h"

// ============================================================================
// Global state (mirrors source/main.c)
// ============================================================================

static int is_running = 1;
static volatile sig_atomic_t got_sigint = 0;

// Console rendering stubs — on host, print/printErr go to stdout/stderr
static PrintConsole *print_console = NULL;

void nx_console_init(nx_context_t *nx_ctx) {
	nx_ctx->rendering_mode = NX_RENDERING_MODE_CONSOLE;
	// On host, stdout is always available — no console init needed
}

void nx_console_exit(void) {
	// No-op on host
}

void nx_framebuffer_exit(void) {
	// No-op on host — no display hardware
}

void nx_render_loading_image(nx_context_t *nx_ctx, const char *nro_path) {
	(void)nx_ctx;
	(void)nro_path;
	// No-op on host — no display hardware
}

// ============================================================================
// Framebuffer init stub (JS-callable)
// ============================================================================

static JSValue nx_framebuffer_init(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
	(void)this_val;
	(void)argc;
	(void)argv;
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	nx_ctx->rendering_mode = NX_RENDERING_MODE_CANVAS;
	return JS_UNDEFINED;
}

// ============================================================================
// Exit handler
// ============================================================================

static JSValue js_exit(JSContext *ctx, JSValueConst this_val, int argc,
                       JSValueConst *argv) {
	(void)ctx;
	(void)this_val;
	(void)argc;
	(void)argv;
	is_running = 0;
	return JS_UNDEFINED;
}

void nx_exit_event_loop(void) {
	is_running = 0;
}

// ============================================================================
// Debug / instrumentation functions (test binary only)
// ============================================================================

static JSValue js_gc(JSContext *ctx, JSValueConst this_val, int argc,
                     JSValueConst *argv) {
	(void)this_val;
	(void)argc;
	(void)argv;
	JS_RunGC(JS_GetRuntime(ctx));
	return JS_UNDEFINED;
}

static JSValue js_memory_usage(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
	(void)this_val;
	(void)argc;
	(void)argv;
	JSMemoryUsage stats;
	JS_ComputeMemoryUsage(JS_GetRuntime(ctx), &stats);

	JSValue obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, obj, "mallocSize",
	                  JS_NewInt64(ctx, stats.malloc_size));
	JS_SetPropertyStr(ctx, obj, "mallocCount",
	                  JS_NewInt64(ctx, stats.malloc_count));
	JS_SetPropertyStr(ctx, obj, "memoryUsedSize",
	                  JS_NewInt64(ctx, stats.memory_used_size));
	JS_SetPropertyStr(ctx, obj, "memoryUsedCount",
	                  JS_NewInt64(ctx, stats.memory_used_count));
	JS_SetPropertyStr(ctx, obj, "objCount",
	                  JS_NewInt64(ctx, stats.obj_count));
	JS_SetPropertyStr(ctx, obj, "objSize",
	                  JS_NewInt64(ctx, stats.obj_size));
	JS_SetPropertyStr(ctx, obj, "strCount",
	                  JS_NewInt64(ctx, stats.str_count));
	JS_SetPropertyStr(ctx, obj, "strSize",
	                  JS_NewInt64(ctx, stats.str_size));
	JS_SetPropertyStr(ctx, obj, "binaryObjectCount",
	                  JS_NewInt64(ctx, stats.binary_object_count));
	JS_SetPropertyStr(ctx, obj, "binaryObjectSize",
	                  JS_NewInt64(ctx, stats.binary_object_size));
	JS_SetPropertyStr(ctx, obj, "propCount",
	                  JS_NewInt64(ctx, stats.prop_count));
	JS_SetPropertyStr(ctx, obj, "shapeCount",
	                  JS_NewInt64(ctx, stats.shape_count));
	JS_SetPropertyStr(ctx, obj, "jsFuncCount",
	                  JS_NewInt64(ctx, stats.js_func_count));
	JS_SetPropertyStr(ctx, obj, "arrayCount",
	                  JS_NewInt64(ctx, stats.array_count));
	return obj;
}

// ============================================================================
// Print functions
// ============================================================================

static JSValue js_print(JSContext *ctx, JSValueConst this_val, int argc,
                        JSValueConst *argv) {
	(void)this_val;
	const char *str = JS_ToCString(ctx, argv[0]);
	if (!str)
		return JS_EXCEPTION;
	printf("%s", str);
	JS_FreeCString(ctx, str);
	return JS_UNDEFINED;
}

static JSValue js_print_err(JSContext *ctx, JSValueConst this_val, int argc,
                            JSValueConst *argv) {
	(void)this_val;
	const char *str = JS_ToCString(ctx, argv[0]);
	if (!str)
		return JS_EXCEPTION;
	fprintf(stderr, "%s", str);
	JS_FreeCString(ctx, str);
	return JS_UNDEFINED;
}

// ============================================================================
// CWD / chdir
// ============================================================================

static JSValue js_cwd(JSContext *ctx, JSValueConst this_val, int argc,
                      JSValueConst *argv) {
	(void)this_val;
	(void)argc;
	(void)argv;
	char cwd[4096];
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		// Ensure trailing slash for URL compatibility
		size_t len = strlen(cwd);
		if (len > 0 && cwd[len - 1] != '/') {
			cwd[len] = '/';
			cwd[len + 1] = '\0';
		}
		return JS_NewString(ctx, cwd);
	}
	return JS_UNDEFINED;
}

static JSValue js_chdir(JSContext *ctx, JSValueConst this_val, int argc,
                        JSValueConst *argv) {
	(void)this_val;
	const char *dir = JS_ToCString(ctx, argv[0]);
	if (!dir)
		return JS_EXCEPTION;
	if (chdir(dir) != 0) {
		JS_ThrowTypeError(ctx, "%s: %s", strerror(errno), dir);
		JS_FreeCString(ctx, dir);
		return JS_EXCEPTION;
	}
	JS_FreeCString(ctx, dir);
	return JS_UNDEFINED;
}

// ============================================================================
// HID stubs (JS-callable — return empty/no-op results on host)
// ============================================================================

static JSValue js_hid_initialize_touch_screen(JSContext *ctx,
                                              JSValueConst this_val, int argc,
                                              JSValueConst *argv) {
	(void)ctx;
	(void)this_val;
	(void)argc;
	(void)argv;
	return JS_UNDEFINED;
}

static JSValue js_hid_initialize_keyboard(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv) {
	(void)ctx;
	(void)this_val;
	(void)argc;
	(void)argv;
	return JS_UNDEFINED;
}

static JSValue js_hid_initialize_vibration_devices(JSContext *ctx,
                                                   JSValueConst this_val,
                                                   int argc,
                                                   JSValueConst *argv) {
	(void)ctx;
	(void)this_val;
	(void)argc;
	(void)argv;
	return JS_UNDEFINED;
}

static JSValue js_hid_send_vibration_values(JSContext *ctx,
                                            JSValueConst this_val, int argc,
                                            JSValueConst *argv) {
	(void)ctx;
	(void)this_val;
	(void)argc;
	(void)argv;
	return JS_UNDEFINED;
}

static JSValue js_hid_get_touch_screen_states(JSContext *ctx,
                                              JSValueConst this_val, int argc,
                                              JSValueConst *argv) {
	(void)this_val;
	(void)argc;
	(void)argv;
	// Return undefined — no touches on host
	return JS_UNDEFINED;
}

static JSValue js_hid_get_keyboard_states(JSContext *ctx, JSValueConst this_val,
                                          int argc, JSValueConst *argv) {
	(void)this_val;
	(void)argc;
	(void)argv;
	// Return empty keyboard state
	JSValue obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, obj, "modifiers", JS_NewBigUint64(ctx, 0));
	for (int i = 0; i < 4; i++) {
		JS_SetPropertyUint32(ctx, obj, i, JS_NewBigUint64(ctx, 0));
	}
	return obj;
}

// ============================================================================
// Environment variable functions
// ============================================================================

static JSValue js_getenv(JSContext *ctx, JSValueConst this_val, int argc,
                         JSValueConst *argv) {
	(void)this_val;
	const char *name = JS_ToCString(ctx, argv[0]);
	if (!name)
		return JS_EXCEPTION;
	char *value = getenv(name);
	JS_FreeCString(ctx, name);
	if (value == NULL) {
		return JS_UNDEFINED;
	}
	return JS_NewString(ctx, value);
}

static JSValue js_setenv(JSContext *ctx, JSValueConst this_val, int argc,
                         JSValueConst *argv) {
	(void)this_val;
	const char *name = JS_ToCString(ctx, argv[0]);
	if (!name)
		return JS_EXCEPTION;
	const char *value = JS_ToCString(ctx, argv[1]);
	if (!value) {
		JS_FreeCString(ctx, name);
		return JS_EXCEPTION;
	}
	if (setenv(name, value, 1) != 0) {
		JS_ThrowTypeError(ctx, "%s: %s=%s", strerror(errno), name, value);
		JS_FreeCString(ctx, name);
		JS_FreeCString(ctx, value);
		return JS_EXCEPTION;
	}
	JS_FreeCString(ctx, name);
	JS_FreeCString(ctx, value);
	return JS_UNDEFINED;
}

static JSValue js_unsetenv(JSContext *ctx, JSValueConst this_val, int argc,
                           JSValueConst *argv) {
	(void)this_val;
	const char *name = JS_ToCString(ctx, argv[0]);
	if (!name)
		return JS_EXCEPTION;
	if (unsetenv(name) != 0) {
		JS_ThrowTypeError(ctx, "%s: %s", strerror(errno), name);
		JS_FreeCString(ctx, name);
		return JS_EXCEPTION;
	}
	JS_FreeCString(ctx, name);
	return JS_UNDEFINED;
}

static JSValue js_env_to_object(JSContext *ctx, JSValueConst this_val, int argc,
                                JSValueConst *argv) {
	(void)this_val;
	(void)argc;
	(void)argv;
	JSValue env = JS_NewObject(ctx);
	extern char **environ;
	char **envp = environ;
	while (*envp) {
		char *key = strdup(*envp);
		char *eq = strchr(key, '=');
		if (eq) {
			*eq = '\0';
			char *value = eq + 1;
			JS_SetPropertyStr(ctx, env, key, JS_NewString(ctx, value));
		}
		free(key);
		envp++;
	}
	return env;
}

// ============================================================================
// Promise state introspection
// ============================================================================

static JSValue js_get_internal_promise_state(JSContext *ctx,
                                             JSValueConst this_val, int argc,
                                             JSValueConst *argv) {
	(void)this_val;
	JSPromiseStateEnum state = JS_PromiseState(ctx, argv[0]);
	JSValue arr = JS_NewArray(ctx);
	JS_SetPropertyUint32(ctx, arr, 0, JS_NewUint32(ctx, state));
	if (state > JS_PROMISE_PENDING) {
		JS_SetPropertyUint32(ctx, arr, 1, JS_PromiseResult(ctx, argv[0]));
	} else {
		JS_SetPropertyUint32(ctx, arr, 1, JS_NULL);
	}
	return arr;
}

// ============================================================================
// Frame and exit handler setters
// ============================================================================

static JSValue nx_set_frame_handler(JSContext *ctx, JSValueConst this_val,
                                    int argc, JSValueConst *argv) {
	(void)this_val;
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	nx_ctx->frame_handler = JS_DupValue(ctx, argv[0]);
	return JS_UNDEFINED;
}

static JSValue nx_set_exit_handler(JSContext *ctx, JSValueConst this_val,
                                   int argc, JSValueConst *argv) {
	(void)this_val;
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	nx_ctx->exit_handler = JS_DupValue(ctx, argv[0]);
	return JS_UNDEFINED;
}

// ============================================================================
// nx_module_set_import_meta (from source/main.c — needed for ES module loading)
// ============================================================================

int nx_module_set_import_meta(JSContext *ctx, JSValueConst func_val,
                              const char *url, bool is_main) {
	JSModuleDef *m = JS_VALUE_GET_PTR(func_val);
	JSValue meta_obj = JS_GetImportMeta(ctx, m);
	if (JS_IsException(meta_obj))
		return -1;
	JS_DefinePropertyValueStr(ctx, meta_obj, "url", JS_NewString(ctx, url),
	                          JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, meta_obj, "main", JS_NewBool(ctx, is_main),
	                          JS_PROP_C_W_E);
	JS_FreeValue(ctx, meta_obj);
	return 0;
}

// ============================================================================
// Pending job processing (from source/main.c)
// ============================================================================

void nx_process_pending_jobs(JSContext *ctx, nx_context_t *nx_ctx,
                             JSRuntime *rt) {
	JSContext *ctx1;
	int err;
	// Process up to 20 pending jobs per frame (same as real Switch build)
	for (u8 i = 0; i < 20; i++) {
		err = JS_ExecutePendingJob(rt, &ctx1);
		if (err <= 0) {
			if (err < 0) {
				nx_emit_error_event(ctx1);
			}
			break;
		}
	}

	if (!JS_IsUndefined(nx_ctx->unhandled_rejected_promise)) {
		nx_emit_unhandled_rejection_event(ctx);
	}
}

// ============================================================================
// File reading helper
// ============================================================================

static char *read_file_text(const char *path, size_t *out_len) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);

	char *buf = malloc(len + 1);
	if (!buf) {
		fclose(f);
		return NULL;
	}
	size_t rd = fread(buf, 1, len, f);
	fclose(f);
	buf[rd] = '\0';
	if (out_len)
		*out_len = rd;
	return buf;
}

// Also provide the read_file function that source/main.c would define
// (used by some source files that reference it externally)
uint8_t *read_file(const char *filename, size_t *out_size) {
	FILE *file = fopen(filename, "rb");
	if (file == NULL) {
		return NULL;
	}
	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	rewind(file);
	uint8_t *buffer = malloc(size + 1);
	if (buffer == NULL) {
		fclose(file);
		return NULL;
	}
	size_t result = fread(buffer, 1, size, file);
	fclose(file);
	if (result != size) {
		free(buffer);
		return NULL;
	}
	*out_size = size;
	buffer[size] = '\0';
	return buffer;
}

// ============================================================================
// Host font override (from canvas test — loads a system TTF on the host)
// ============================================================================

static void setup_host_font(JSContext *ctx) {
	const char *font_paths[] = {
	    // macOS
	    "/System/Library/Fonts/Helvetica.ttc",
	    "/System/Library/Fonts/SFNS.ttf",
	    "/Library/Fonts/Arial.ttf",
	    // Linux common paths
	    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
	    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
	    "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
	    "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf",
	    "/usr/share/fonts/TTF/DejaVuSans.ttf",
	    NULL,
	};

	size_t font_len = 0;
	char *font_data = NULL;
	for (int i = 0; font_paths[i]; i++) {
		FILE *f = fopen(font_paths[i], "rb");
		if (!f)
			continue;
		fseek(f, 0, SEEK_END);
		long len = ftell(f);
		fseek(f, 0, SEEK_SET);
		font_data = malloc(len);
		if (font_data) {
			font_len = fread(font_data, 1, len, f);
		}
		fclose(f);
		if (font_data)
			break;
	}

	if (font_data) {
		JSValue font_ab =
		    JS_NewArrayBufferCopy(ctx, (uint8_t *)font_data, font_len);
		JSValue global = JS_GetGlobalObject(ctx);
		JS_SetPropertyStr(ctx, global, "__hostFallbackFont__", font_ab);
		JS_FreeValue(ctx, global);
		free(font_data);

		static const char *FONT_OVERRIDE =
		    "$.getSystemFont = function() { return __hostFallbackFont__; };\n";
		JSValue fo_val =
		    JS_Eval(ctx, FONT_OVERRIDE, strlen(FONT_OVERRIDE),
		            "<font-override>", JS_EVAL_TYPE_GLOBAL);
		if (JS_IsException(fo_val)) {
			fprintf(stderr,
			        "Warning: font override failed, font tests may fail\n");
			JSValue ex = JS_GetException(ctx);
			const char *str = JS_ToCString(ctx, ex);
			if (str) {
				fprintf(stderr, "%s\n", str);
				JS_FreeCString(ctx, str);
			}
			JS_FreeValue(ctx, ex);
		}
		JS_FreeValue(ctx, fo_val);
	} else {
		fprintf(stderr,
		        "Warning: no fallback font found — getSystemFont will fail\n");
	}
}

// ============================================================================
// Proxy stub (catches missing native functions as no-ops)
// ============================================================================

static const char *PROXY_STUB =
    "globalThis.$ = new Proxy($, {\n"
    "    get: function(target, prop) {\n"
    "        if (prop in target) return target[prop];\n"
    "        return function() { return {}; };\n"
    "    }\n"
    "});\n";

// ============================================================================
// SIGINT handler for graceful shutdown
// ============================================================================

static void sigint_handler(int sig) {
	(void)sig;
	got_sigint = 1;
}

// ============================================================================
// Main entry point
// ============================================================================

int main(int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "Usage: %s <runtime.js> <script.js> [args...]\n",
		        argv[0]);
		return 1;
	}

	const char *runtime_path_arg = argv[1];
	const char *script_path = argv[2];

	// Resolve runtime path to an absolute file:// URL for stack traces
	char runtime_path[4096];
	if (runtime_path_arg[0] == '/') {
		snprintf(runtime_path, sizeof(runtime_path), "file://%s",
		         runtime_path_arg);
	} else {
		char cwd[4096];
		if (getcwd(cwd, sizeof(cwd))) {
			snprintf(runtime_path, sizeof(runtime_path), "file://%s/%s", cwd,
			         runtime_path_arg);
		} else {
			snprintf(runtime_path, sizeof(runtime_path), "file://%s",
			         runtime_path_arg);
		}
	}

	// Set up SIGINT handler for graceful shutdown
	struct sigaction sa;
	sa.sa_handler = sigint_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);

	// Initialize PSA Crypto — required by mbedtls 3.x for TLS 1.3 key
	// exchange. Without this, psa_generate_key() fails during the TLS
	// handshake with MBEDTLS_ERR_SSL_INTERNAL_ERROR.
	psa_status_t psa_status = psa_crypto_init();
	if (psa_status != PSA_SUCCESS) {
		fprintf(stderr, "psa_crypto_init() failed: %d\n", (int)psa_status);
		return 1;
	}

	// Initialize nx_context
	nx_context_t *nx_ctx = malloc(sizeof(nx_context_t));
	memset(nx_ctx, 0, sizeof(nx_context_t));

	// Pre-load host system CA certificates so TLS verification works.
	// This sets ca_certs_loaded=true so tls.c's nx_tls_load_ca_certs()
	// (which uses the Switch SSL service) is bypassed entirely.
	{
		mbedtls_x509_crt_init(&nx_ctx->ca_chain);
		int ca_loaded = 0;

		// Try common CA bundle paths
		const char *ca_paths[] = {
		    // macOS
		    "/etc/ssl/cert.pem",
		    "/opt/homebrew/etc/openssl@3/cert.pem",
		    "/opt/homebrew/etc/ca-certificates/cert.pem",
		    "/usr/local/etc/openssl@3/cert.pem",
		    // Linux
		    "/etc/ssl/certs/ca-certificates.crt",
		    "/etc/pki/tls/certs/ca-bundle.crt",
		    "/etc/ssl/ca-bundle.pem",
		    NULL,
		};
		for (int i = 0; ca_paths[i]; i++) {
			int ret =
			    mbedtls_x509_crt_parse_file(&nx_ctx->ca_chain, ca_paths[i]);
			if (ret >= 0) {
				ca_loaded = 1;
				break;
			}
		}

		if (ca_loaded) {
			nx_ctx->ca_certs_loaded = true;
		} else {
			fprintf(stderr,
			        "Warning: could not load system CA certificates; "
			        "TLS verification will be disabled\n");
		}
	}

	// Create QuickJS runtime and context
	JSRuntime *rt = JS_NewRuntime();
	if (!rt) {
		fprintf(stderr, "Failed to create JS runtime\n");
		free(nx_ctx);
		return 1;
	}

	// Set JS heap memory limit. Use NXJS_MEMORY_LIMIT env var (in bytes)
	// to simulate constrained environments like Nintendo Switch hardware.
	// Default: unlimited. Example: NXJS_MEMORY_LIMIT=268435456 (256 MB)
	const char *mem_limit_str = getenv("NXJS_MEMORY_LIMIT");
	if (mem_limit_str) {
		size_t mem_limit = (size_t)strtoull(mem_limit_str, NULL, 10);
		if (mem_limit > 0) {
			JS_SetMemoryLimit(rt, mem_limit);
			fprintf(stderr, "JS memory limit set to %zu bytes (%.1f MB)\n",
			        mem_limit, (double)mem_limit / (1024 * 1024));
		}
	}

	JSContext *ctx = JS_NewContext(rt);
	if (!ctx) {
		fprintf(stderr, "Failed to create JS context\n");
		JS_FreeRuntime(rt);
		free(nx_ctx);
		return 1;
	}

	// Initialize thread pool and async infrastructure
	nx_ctx->rendering_mode = NX_RENDERING_MODE_INIT;
	nx_ctx->thpool = thpool_init(4);
	nx_ctx->frame_handler = JS_UNDEFINED;
	nx_ctx->exit_handler = JS_UNDEFINED;
	nx_ctx->error_handler = JS_UNDEFINED;
	nx_ctx->unhandled_rejection_handler = JS_UNDEFINED;
	nx_ctx->unhandled_rejected_promise = JS_UNDEFINED;
	pthread_mutex_init(&(nx_ctx->async_done_mutex), NULL);
	nx_poll_init(&nx_ctx->poll);
	JS_SetContextOpaque(ctx, nx_ctx);
	JS_SetRuntimeOpaque(rt, nx_ctx);
	JS_SetHostPromiseRejectionTracker(rt, nx_promise_rejection_handler, ctx);

	// Build the init object ($) with ALL native modules
	JSValue global_obj = JS_GetGlobalObject(ctx);
	nx_ctx->init_obj = JS_NewObject(ctx);

	// Real modules (compiled from source/*.c)
	nx_init_canvas(ctx, nx_ctx->init_obj);
	nx_init_compression(ctx, nx_ctx->init_obj);
	nx_init_crypto(ctx, nx_ctx->init_obj);
	nx_init_dns(ctx, nx_ctx->init_obj);
	nx_init_dommatrix(ctx, nx_ctx->init_obj);
	nx_init_error(ctx, nx_ctx->init_obj);
	nx_init_font(ctx, nx_ctx->init_obj);
	nx_init_image(ctx, nx_ctx->init_obj);
	nx_init_tcp(ctx, nx_ctx->init_obj);
	nx_init_tls(ctx, nx_ctx->init_obj);
	nx_init_udp(ctx, nx_ctx->init_obj);
	nx_init_uint8array(ctx, nx_ctx->init_obj);
	nx_init_url(ctx, nx_ctx->init_obj);
	nx_init_wasm(ctx, nx_ctx->init_obj);
	nx_init_web(ctx, nx_ctx->init_obj);
	nx_init_window(ctx, nx_ctx->init_obj);
	nx_init_fs(ctx, nx_ctx->init_obj);

	// Stubbed modules (no-op — defined in stubs.c)
	nx_init_account(ctx, nx_ctx->init_obj);
	nx_init_album(ctx, nx_ctx->init_obj);
	nx_init_applet(ctx, nx_ctx->init_obj);
	nx_init_audio(ctx, nx_ctx->init_obj);
	nx_init_battery(ctx, nx_ctx->init_obj);
	nx_init_fsdev(ctx, nx_ctx->init_obj);
	nx_init_gamepad(ctx, nx_ctx->init_obj);
	nx_init_irs(ctx, nx_ctx->init_obj);
	nx_init_nifm(ctx, nx_ctx->init_obj);
	nx_init_ns(ctx, nx_ctx->init_obj);
	nx_init_service(ctx, nx_ctx->init_obj);
	nx_init_swkbd(ctx, nx_ctx->init_obj);

	// Inline functions (from source/main.c — portable on host)
	const JSCFunctionListEntry init_function_list[] = {
	    JS_CFUNC_DEF("exit", 0, js_exit),
	    JS_CFUNC_DEF("gc", 0, js_gc),
	    JS_CFUNC_DEF("memoryUsage", 0, js_memory_usage),
	    JS_CFUNC_DEF("cwd", 0, js_cwd),
	    JS_CFUNC_DEF("chdir", 1, js_chdir),
	    JS_CFUNC_DEF("print", 1, js_print),
	    JS_CFUNC_DEF("printErr", 1, js_print_err),
	    JS_CFUNC_DEF("getInternalPromiseState", 1,
	                 js_get_internal_promise_state),
	    JS_CFUNC_DEF("hidInitializeTouchScreen", 0,
	                 js_hid_initialize_touch_screen),
	    JS_CFUNC_DEF("hidGetTouchScreenStates", 0,
	                 js_hid_get_touch_screen_states),
	    JS_CFUNC_DEF("getenv", 1, js_getenv),
	    JS_CFUNC_DEF("setenv", 2, js_setenv),
	    JS_CFUNC_DEF("unsetenv", 1, js_unsetenv),
	    JS_CFUNC_DEF("envToObject", 0, js_env_to_object),
	    JS_CFUNC_DEF("onExit", 1, nx_set_exit_handler),
	    JS_CFUNC_DEF("onFrame", 1, nx_set_frame_handler),
	    JS_CFUNC_DEF("framebufferInit", 1, nx_framebuffer_init),
	    JS_CFUNC_DEF("hidInitializeKeyboard", 0, js_hid_initialize_keyboard),
	    JS_CFUNC_DEF("hidInitializeVibrationDevices", 0,
	                 js_hid_initialize_vibration_devices),
	    JS_CFUNC_DEF("hidGetKeyboardStates", 0, js_hid_get_keyboard_states),
	    JS_CFUNC_DEF("hidSendVibrationValues", 0,
	                 js_hid_send_vibration_values),
	};
	JS_SetPropertyFunctionList(ctx, nx_ctx->init_obj, init_function_list,
	                           countof(init_function_list));

	// Switch.version
	JSAtom atom;
	JSValue version_obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, version_obj, "nxjs",
	                  JS_NewString(ctx, NXJS_VERSION));
	JS_SetPropertyStr(ctx, version_obj, "hos", JS_NewString(ctx, "0.0.0"));
	JS_SetPropertyStr(ctx, version_obj, "quickjs",
	                  JS_NewString(ctx, JS_GetVersion()));
	JS_SetPropertyStr(ctx, nx_ctx->init_obj, "version", version_obj);

	// Switch.entrypoint
	// Build a file:// URL from the script path
	char entrypoint[4096];
	if (script_path[0] == '/') {
		snprintf(entrypoint, sizeof(entrypoint), "file://%s", script_path);
	} else {
		char cwd[4096];
		if (getcwd(cwd, sizeof(cwd))) {
			snprintf(entrypoint, sizeof(entrypoint), "file://%s/%s", cwd,
			         script_path);
		} else {
			snprintf(entrypoint, sizeof(entrypoint), "file://%s", script_path);
		}
	}
	JS_SetPropertyStr(ctx, nx_ctx->init_obj, "entrypoint",
	                  JS_NewString(ctx, entrypoint));

	// Switch.argv
	JSValue argv_array = JS_NewArray(ctx);
	for (int i = 2; i < argc; i++) {
		JS_SetPropertyUint32(ctx, argv_array, i - 2,
		                     JS_NewString(ctx, argv[i]));
	}
	JS_SetPropertyStr(ctx, nx_ctx->init_obj, "argv", argv_array);

	// Expose $ as global
	JS_SetPropertyStr(ctx, global_obj, "$", nx_ctx->init_obj);

	// Expose debug/instrumentation functions as globals (test binary only).
	// These survive the `delete globalThis.$` in the runtime and are
	// available to test scripts for memory leak investigation.
	JS_SetPropertyStr(
	    ctx, global_obj, "__gc",
	    JS_NewCFunction(ctx, js_gc, "__gc", 0));
	JS_SetPropertyStr(
	    ctx, global_obj, "__memoryUsage",
	    JS_NewCFunction(ctx, js_memory_usage, "__memoryUsage", 0));

	// Install the Proxy stub so runtime.js can load without crashing
	// even for modules whose native bindings are no-ops
	JSValue proxy_val =
	    JS_Eval(ctx, PROXY_STUB, strlen(PROXY_STUB), "<proxy>",
	            JS_EVAL_TYPE_GLOBAL);
	if (JS_IsException(proxy_val)) {
		fprintf(stderr, "Proxy stub evaluation failed:\n");
		print_js_error(ctx);
		JS_FreeValue(ctx, proxy_val);
		goto cleanup;
	}
	JS_FreeValue(ctx, proxy_val);

	// Override getSystemFont with a host-system font
	setup_host_font(ctx);

	// Load and evaluate runtime.js
	{
		size_t runtime_len;
		char *runtime_src = read_file_text(runtime_path_arg, &runtime_len);
		if (!runtime_src) {
			fprintf(stderr, "Failed to read runtime: %s\n", runtime_path_arg);
			goto cleanup;
		}

		JSValue runtime_val =
		    JS_Eval(ctx, runtime_src, runtime_len, runtime_path,
		            JS_EVAL_TYPE_GLOBAL);
		free(runtime_src);

		if (JS_IsException(runtime_val)) {
			fprintf(stderr, "Runtime initialization failed:\n");
			print_js_error(ctx);
			JS_FreeValue(ctx, runtime_val);
			goto cleanup;
		}
		JS_FreeValue(ctx, runtime_val);
	}

	// Load and evaluate the user script as an ES module
	{
		size_t script_len;
		char *script_src = read_file_text(script_path, &script_len);
		if (!script_src) {
			fprintf(stderr, "Failed to read script: %s (%s)\n", script_path,
			        strerror(errno));
			goto cleanup;
		}

		JSValue script_val =
		    JS_Eval(ctx, script_src, script_len, script_path,
		            JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
		free(script_src);

		if (JS_IsException(script_val)) {
			fprintf(stderr, "Script compilation failed:\n");
			nx_emit_error_event(ctx);
			JS_FreeValue(ctx, script_val);
			goto cleanup;
		}

		nx_module_set_import_meta(ctx, script_val, entrypoint, true);
		script_val = JS_EvalFunction(ctx, script_val);

		if (JS_IsException(script_val)) {
			nx_emit_error_event(ctx);
		}
		JS_FreeValue(ctx, script_val);
	}

	// ================================================================
	// Main event loop — runs at ~60 FPS
	// ================================================================
	{
		const long frame_ns = 16666667; // 1/60th second in nanoseconds

		while (is_running && !got_sigint) {
			struct timespec frame_start;
			clock_gettime(CLOCK_MONOTONIC, &frame_start);

			if (!nx_ctx->had_error) {
				// Check if any file descriptors have reported activity
				nx_poll(&nx_ctx->poll);
			}

			if (!nx_ctx->had_error) {
				// Check if any thread pool tasks have completed
				nx_process_async(ctx, nx_ctx);
			}

			if (!nx_ctx->had_error) {
				// Process any Promises that need to be fulfilled
				nx_process_pending_jobs(ctx, nx_ctx, rt);
			}

			if (nx_ctx->had_error) {
				// On error, break out of the loop (no need to wait for
				// user input like on the Switch)
				break;
			}

			// Call frame handler (no gamepad input on host)
			if (!JS_IsUndefined(nx_ctx->frame_handler)) {
				JSValueConst args[] = {JS_FALSE};
				JSValue ret_val =
				    JS_Call(ctx, nx_ctx->frame_handler, JS_NULL, 1, args);
				if (JS_IsException(ret_val)) {
					nx_emit_error_event(ctx);
				}
				JS_FreeValue(ctx, ret_val);

				if (!is_running)
					break;
			}

			// Sleep to maintain ~60 FPS
			struct timespec frame_end;
			clock_gettime(CLOCK_MONOTONIC, &frame_end);
			long elapsed_ns = (frame_end.tv_sec - frame_start.tv_sec) *
			                      1000000000L +
			                  (frame_end.tv_nsec - frame_start.tv_nsec);
			if (elapsed_ns < frame_ns) {
				struct timespec sleep_time = {
				    .tv_sec = 0, .tv_nsec = frame_ns - elapsed_ns};
				nanosleep(&sleep_time, NULL);
			}
		}
	}

cleanup:
	// Destroy the thread pool — signals threads to stop and joins them.
	// No thpool_wait() here: when the user calls Switch.exit(), the app
	// should exit promptly without waiting for in-flight jobs to finish.
	if (nx_ctx->thpool) {
		thpool_destroy(nx_ctx->thpool);
	}

	// Call exit handler
	if (!JS_IsUndefined(nx_ctx->exit_handler)) {
		JSValue ret_val =
		    JS_Call(ctx, nx_ctx->exit_handler, JS_NULL, 0, NULL);
		JS_FreeValue(ctx, ret_val);
	}

	// Cleanup QuickJS
	JS_FreeValue(ctx, global_obj);
	JS_FreeValue(ctx, nx_ctx->frame_handler);
	JS_FreeValue(ctx, nx_ctx->exit_handler);
	JS_FreeValue(ctx, nx_ctx->error_handler);
	JS_FreeValue(ctx, nx_ctx->unhandled_rejection_handler);

	JS_FreeContext(ctx);
	JS_FreeRuntime(rt);

	// Cleanup native resources
	if (nx_ctx->wasm_env) {
		m3_FreeEnvironment(nx_ctx->wasm_env);
	}
	if (nx_ctx->ft_library) {
		FT_Done_FreeType(nx_ctx->ft_library);
	}
	if (nx_ctx->mbedtls_initialized) {
		mbedtls_ctr_drbg_free(&nx_ctx->ctr_drbg);
		mbedtls_entropy_free(&nx_ctx->entropy);
	}
	if (nx_ctx->ca_certs_loaded) {
		mbedtls_x509_crt_free(&nx_ctx->ca_chain);
	}

	int exit_code = nx_ctx->had_error ? 1 : 0;
	free(nx_ctx);

	return exit_code;
}
