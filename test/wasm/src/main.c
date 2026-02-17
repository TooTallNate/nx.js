/**
 * WebAssembly Test Host
 *
 * Minimal QuickJS host for testing the nx.js WebAssembly implementation
 * on x86_64 without a Nintendo Switch.
 *
 * Uses the real runtime.js instead of a custom JS bridge. A Proxy stub
 * catches missing native functions (from modules we don't link) as no-ops,
 * allowing the full runtime to initialize with only WASM natives.
 *
 * Usage: nxjs-wasm-test <runtime.js> <helpers.js> <fixture.js> <output.json> [modules_dir]
 */

#include "types.h"
#include "wasm.h"
#include <m3_env.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declaration — implemented in stubs.c
void print_js_error(JSContext *ctx);

// Read entire file into a malloc'd buffer, returns NULL on failure
static char *read_file_text(const char *path, size_t *out_len) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "Cannot open file: %s\n", path);
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

// Free callback for JS_NewArrayBuffer
static void js_free_arraybuffer(JSRuntime *rt, void *opaque, void *ptr) {
	js_free_rt(rt, ptr);
}

// Read file into an ArrayBuffer — exposed to JS as readFile(path)
static JSValue js_read_file(JSContext *ctx, JSValueConst this_val,
                            int argc, JSValueConst *argv) {
	const char *path = JS_ToCString(ctx, argv[0]);
	if (!path)
		return JS_EXCEPTION;

	FILE *f = fopen(path, "rb");
	if (!f) {
		JSValue err = JS_ThrowInternalError(ctx, "Cannot open file: %s", path);
		JS_FreeCString(ctx, path);
		return err;
	}

	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);

	uint8_t *buf = js_malloc(ctx, len);
	if (!buf) {
		fclose(f);
		JS_FreeCString(ctx, path);
		return JS_EXCEPTION;
	}

	size_t rd = fread(buf, 1, len, f);
	fclose(f);
	JS_FreeCString(ctx, path);

	return JS_NewArrayBuffer(ctx, buf, rd, js_free_arraybuffer, NULL, 0);
}

// __output(value) — JSON.stringify then write to output file
static const char *g_output_path = NULL;

static JSValue js_output(JSContext *ctx, JSValueConst this_val,
                         int argc, JSValueConst *argv) {
	JSValue global = JS_GetGlobalObject(ctx);
	JSValue json = JS_GetPropertyStr(ctx, global, "JSON");
	JSValue stringify = JS_GetPropertyStr(ctx, json, "stringify");

	JSValue args[3];
	args[0] = argv[0];
	args[1] = JS_NULL;
	args[2] = JS_NewInt32(ctx, 2);

	JSValue result = JS_Call(ctx, stringify, json, 3, args);
	JS_FreeValue(ctx, args[2]);
	JS_FreeValue(ctx, stringify);
	JS_FreeValue(ctx, json);
	JS_FreeValue(ctx, global);

	if (JS_IsException(result))
		return JS_EXCEPTION;

	const char *str = JS_ToCString(ctx, result);
	JS_FreeValue(ctx, result);
	if (!str)
		return JS_EXCEPTION;

	if (g_output_path) {
		FILE *f = fopen(g_output_path, "w");
		if (f) {
			fputs(str, f);
			fputs("\n", f);
			fclose(f);
		} else {
			fprintf(stderr, "Cannot write output: %s\n", g_output_path);
		}
	} else {
		printf("%s\n", str);
	}

	JS_FreeCString(ctx, str);
	return JS_UNDEFINED;
}

/**
 * Evaluate a JS file. Returns 0 on success, -1 on error.
 */
static int eval_file(JSContext *ctx, const char *path, const char *label) {
	size_t len;
	char *src = read_file_text(path, &len);
	if (!src) {
		fprintf(stderr, "Failed to read %s: %s\n", label, path);
		return -1;
	}

	JSValue val = JS_Eval(ctx, src, len, path, JS_EVAL_TYPE_GLOBAL);
	free(src);

	if (JS_IsException(val)) {
		fprintf(stderr, "%s evaluation failed:\n", label);
		print_js_error(ctx);
		JS_FreeValue(ctx, val);
		return -1;
	}
	JS_FreeValue(ctx, val);
	return 0;
}

/**
 * Proxy stub: wraps $ so that any missing native function returns a no-op
 * that returns an empty object (needed so runtime.js can initialize modules
 * we don't link — e.g. canvas, compression — without crashing).
 */
static const char *PROXY_STUB =
    "globalThis.$ = new Proxy($, {\n"
    "    get: function(target, prop) {\n"
    "        if (prop in target) return target[prop];\n"
    "        return function() { return {}; };\n"
    "    }\n"
    "});\n";

int main(int argc, char *argv[]) {
	if (argc < 5) {
		fprintf(stderr,
		        "Usage: %s <runtime.js> <helpers.js> <fixture.js> <output.json> [modules_dir]\n",
		        argv[0]);
		return 1;
	}

	const char *runtime_path = argv[1];
	const char *helpers_path = argv[2];
	const char *fixture_path = argv[3];
	g_output_path = argv[4];
	const char *modules_dir = argc > 5 ? argv[5] : ".";

	// Initialize nx_context
	nx_context_t nx_ctx;
	memset(&nx_ctx, 0, sizeof(nx_ctx));
	nx_ctx.wasm_env = NULL; // Lazily initialized by wasm.c

	// Initialize QuickJS
	JSRuntime *rt = JS_NewRuntime();
	if (!rt) {
		fprintf(stderr, "Failed to create JS runtime\n");
		return 1;
	}

	JSContext *ctx = JS_NewContext(rt);
	if (!ctx) {
		fprintf(stderr, "Failed to create JS context\n");
		JS_FreeRuntime(rt);
		return 1;
	}

	// Set up context opaque (nx_context_t)
	nx_ctx.init_obj = JS_NewObject(ctx);
	nx_ctx.frame_handler = JS_UNDEFINED;
	nx_ctx.exit_handler = JS_UNDEFINED;
	nx_ctx.error_handler = JS_UNDEFINED;
	nx_ctx.unhandled_rejection_handler = JS_UNDEFINED;
	nx_ctx.unhandled_rejected_promise = JS_UNDEFINED;
	JS_SetContextOpaque(ctx, &nx_ctx);

	// Register native WASM bindings on init_obj
	nx_init_wasm(ctx, nx_ctx.init_obj);

	// Set version, entrypoint, argv on init_obj (runtime.js reads these)
	JSValue version_obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, version_obj, "nxjs",
	                  JS_NewString(ctx, "0.0.0-test"));
	JS_SetPropertyStr(ctx, version_obj, "hos", JS_NewString(ctx, "0.0.0"));
	JS_SetPropertyStr(ctx, nx_ctx.init_obj, "version", version_obj);
	JS_SetPropertyStr(ctx, nx_ctx.init_obj, "entrypoint",
	                  JS_NewString(ctx, "file:///test.js"));
	JS_SetPropertyStr(ctx, nx_ctx.init_obj, "argv", JS_NewArray(ctx));

	// Expose init_obj as global '$'
	JSValue global = JS_GetGlobalObject(ctx);
	JS_SetPropertyStr(ctx, global, "$", JS_DupValue(ctx, nx_ctx.init_obj));
	JS_FreeValue(ctx, global);

	// Evaluate the Proxy stub (catches missing native functions as no-ops)
	JSValue proxy_val =
	    JS_Eval(ctx, PROXY_STUB, strlen(PROXY_STUB), "<proxy>",
	            JS_EVAL_TYPE_GLOBAL);
	if (JS_IsException(proxy_val)) {
		fprintf(stderr, "Proxy stub evaluation failed:\n");
		print_js_error(ctx);
		JS_FreeValue(ctx, proxy_val);
		JS_FreeContext(ctx);
		JS_FreeRuntime(rt);
		return 1;
	}
	JS_FreeValue(ctx, proxy_val);

	// Load and evaluate runtime.js
	if (eval_file(ctx, runtime_path, "runtime") != 0) {
		JS_FreeContext(ctx);
		JS_FreeRuntime(rt);
		return 1;
	}

	// Expose test-specific globals AFTER runtime loads
	global = JS_GetGlobalObject(ctx);

	// readFile() for loading .wasm binaries
	JS_SetPropertyStr(ctx, global, "readFile",
	                  JS_NewCFunction(ctx, js_read_file, "readFile", 1));

	// __output() for writing JSON results
	JS_SetPropertyStr(ctx, global, "__output",
	                  JS_NewCFunction(ctx, js_output, "__output", 1));

	// __modules_dir so helpers/fixtures know where .wasm files live
	JS_SetPropertyStr(ctx, global, "__modules_dir",
	                  JS_NewString(ctx, modules_dir));

	JS_FreeValue(ctx, global);

	// Load and evaluate test helpers
	if (eval_file(ctx, helpers_path, "helpers") != 0) {
		JS_FreeContext(ctx);
		JS_FreeRuntime(rt);
		return 1;
	}

	// Provide loadWasm() for test fixtures (reads .wasm files from modules_dir)
	{
		static const char *load_wasm_fn =
		    "globalThis.loadWasm = function(filename) {\n"
		    "    return readFile(__modules_dir + '/' + filename);\n"
		    "};\n";
		JSValue lw_val =
		    JS_Eval(ctx, load_wasm_fn, strlen(load_wasm_fn), "<loadWasm>",
		            JS_EVAL_TYPE_GLOBAL);
		if (JS_IsException(lw_val)) {
			fprintf(stderr, "loadWasm setup failed:\n");
			print_js_error(ctx);
			JS_FreeValue(ctx, lw_val);
			JS_FreeContext(ctx);
			JS_FreeRuntime(rt);
			return 1;
		}
		JS_FreeValue(ctx, lw_val);
	}

	// Load and evaluate the test fixture
	if (eval_file(ctx, fixture_path, "fixture") != 0) {
		JS_FreeContext(ctx);
		JS_FreeRuntime(rt);
		return 1;
	}

	// Process pending jobs (promise callbacks) until none remain.
	JSContext *ctx1;
	int pending;
	do {
		pending = JS_ExecutePendingJob(rt, &ctx1);
		if (pending < 0) {
			fprintf(stderr, "Error in pending job:\n");
			print_js_error(ctx);
			break;
		}
	} while (pending > 0);

	// Cleanup
	if (nx_ctx.wasm_env) {
		m3_FreeEnvironment(nx_ctx.wasm_env);
	}
	JS_FreeValue(ctx, nx_ctx.init_obj);
	JS_FreeContext(ctx);
	JS_FreeRuntime(rt);

	return 0;
}
