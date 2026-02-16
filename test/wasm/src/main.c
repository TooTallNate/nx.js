/**
 * WebAssembly Test Host
 *
 * Minimal QuickJS host for testing the nx.js WebAssembly implementation
 * on x86_64 without a Nintendo Switch.
 *
 * Usage: nxjs-wasm-test <bridge.js> <fixture.js> <output.json> [modules_dir]
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

int main(int argc, char *argv[]) {
	if (argc < 4) {
		fprintf(stderr,
		        "Usage: %s <bridge.js> <fixture.js> <output.json> [modules_dir]\n",
		        argv[0]);
		return 1;
	}

	const char *bridge_path = argv[1];
	const char *fixture_path = argv[2];
	g_output_path = argv[3];
	const char *modules_dir = argc > 4 ? argv[4] : ".";

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

	// Register native WASM bindings
	nx_init_wasm(ctx, nx_ctx.init_obj);

	// Expose init_obj as global '$' so the JS bridge can access native functions
	JSValue global = JS_GetGlobalObject(ctx);
	JS_SetPropertyStr(ctx, global, "$", JS_DupValue(ctx, nx_ctx.init_obj));

	// Expose readFile() for loading .wasm binaries
	JS_SetPropertyStr(ctx, global, "readFile",
	                  JS_NewCFunction(ctx, js_read_file, "readFile", 1));

	// Expose __output() for writing JSON results
	JS_SetPropertyStr(ctx, global, "__output",
	                  JS_NewCFunction(ctx, js_output, "__output", 1));

	// Expose __modules_dir so the bridge knows where .wasm files live
	JS_SetPropertyStr(ctx, global, "__modules_dir",
	                  JS_NewString(ctx, modules_dir));

	JS_FreeValue(ctx, global);

	// Load and evaluate the JS bridge
	size_t bridge_len;
	char *bridge_src = read_file_text(bridge_path, &bridge_len);
	if (!bridge_src) {
		fprintf(stderr, "Failed to read bridge: %s\n", bridge_path);
		JS_FreeContext(ctx);
		JS_FreeRuntime(rt);
		return 1;
	}

	JSValue bridge_val =
	    JS_Eval(ctx, bridge_src, bridge_len, bridge_path, JS_EVAL_TYPE_GLOBAL);
	free(bridge_src);

	if (JS_IsException(bridge_val)) {
		fprintf(stderr, "Bridge evaluation failed:\n");
		print_js_error(ctx);
		JS_FreeValue(ctx, bridge_val);
		JS_FreeContext(ctx);
		JS_FreeRuntime(rt);
		return 1;
	}
	JS_FreeValue(ctx, bridge_val);

	// Load and evaluate the test fixture
	size_t fixture_len;
	char *fixture_src = read_file_text(fixture_path, &fixture_len);
	if (!fixture_src) {
		fprintf(stderr, "Failed to read fixture: %s\n", fixture_path);
		JS_FreeContext(ctx);
		JS_FreeRuntime(rt);
		return 1;
	}

	JSValue fixture_val = JS_Eval(ctx, fixture_src, fixture_len, fixture_path,
	                              JS_EVAL_TYPE_GLOBAL);
	free(fixture_src);

	if (JS_IsException(fixture_val)) {
		fprintf(stderr, "Fixture evaluation failed:\n");
		print_js_error(ctx);
		JS_FreeValue(ctx, fixture_val);
		JS_FreeContext(ctx);
		JS_FreeRuntime(rt);
		return 1;
	}
	JS_FreeValue(ctx, fixture_val);

	// Cleanup
	if (nx_ctx.wasm_env) {
		m3_FreeEnvironment(nx_ctx.wasm_env);
	}
	JS_FreeValue(ctx, nx_ctx.init_obj);
	JS_FreeContext(ctx);
	JS_FreeRuntime(rt);

	return 0;
}
