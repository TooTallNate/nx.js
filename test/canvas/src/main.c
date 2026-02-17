/**
 * Canvas 2D Test Host
 *
 * Minimal QuickJS host for testing the nx.js Canvas 2D implementation
 * on x86_64 without a Nintendo Switch.
 *
 * Uses the real runtime.js instead of a custom JS bridge. A Proxy stub
 * catches missing native functions (from modules we don't link) as no-ops,
 * allowing the full runtime to initialize with only canvas/font/image/
 * dommatrix natives.
 *
 * Usage: nxjs-canvas-test <runtime.js> <fixture.js> <output.png> [width] [height]
 */

// Include the real source headers — compat/ directory provides stubs for
// <switch.h>, <wasm3.h>, <mbedtls/*.h> that source/types.h pulls in.
#include "types.h"
#include "canvas.h"
#include "dommatrix.h"
#include "error.h"
#include "font.h"
#include "image.h"
#include <cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Read entire file into a malloc'd buffer, returns NULL on failure
static char *read_file(const char *path, size_t *out_len) {
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
	size_t read = fread(buf, 1, len, f);
	fclose(f);
	buf[read] = '\0';
	if (out_len)
		*out_len = read;
	return buf;
}

// Extract canvas surface from the QuickJS global __nxjs_surface__ pointer
static cairo_surface_t *get_canvas_surface(JSContext *ctx) {
	JSValue global = JS_GetGlobalObject(ctx);
	JSValue surface_val = JS_GetPropertyStr(ctx, global, "__nxjs_surface__");
	JS_FreeValue(ctx, global);

	if (JS_IsUndefined(surface_val) || JS_IsNull(surface_val)) {
		JS_FreeValue(ctx, surface_val);
		return NULL;
	}

	// The surface pointer was stored as an opaque via the canvas object
	// We get it through the canvas's nx_canvas_t
	nx_canvas_t *canvas = nx_get_canvas(ctx, surface_val);
	JS_FreeValue(ctx, surface_val);

	if (!canvas || !canvas->surface) {
		return NULL;
	}
	return canvas->surface;
}

/**
 * Evaluate a JS file. Returns 0 on success, -1 on error.
 */
static int eval_file(JSContext *ctx, const char *path, const char *label) {
	size_t len;
	char *src = read_file(path, &len);
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
 * we don't link — e.g. wasm, compression — without crashing).
 */
static const char *PROXY_STUB =
    "globalThis.$ = new Proxy($, {\n"
    "    get: function(target, prop) {\n"
    "        if (prop in target) return target[prop];\n"
    "        return function() { return {}; };\n"
    "    }\n"
    "});\n";

int main(int argc, char *argv[]) {
	if (argc < 4) {
		fprintf(stderr,
		        "Usage: %s <runtime.js> <fixture.js> <output.png> [width] "
		        "[height]\n",
		        argv[0]);
		return 1;
	}

	const char *runtime_path = argv[1];
	const char *fixture_path = argv[2];
	const char *output_path = argv[3];
	int canvas_width = argc > 4 ? atoi(argv[4]) : 200;
	int canvas_height = argc > 5 ? atoi(argv[5]) : 200;

	// Initialize nx_context
	nx_context_t nx_ctx;
	memset(&nx_ctx, 0, sizeof(nx_ctx));
	nx_ctx.ft_library = NULL; // Lazily initialized by font.c

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

	// Set up context opaque (nx_context_t) — this is how canvas.c accesses
	// FreeType library and init_obj
	nx_ctx.init_obj = JS_NewObject(ctx);
	nx_ctx.frame_handler = JS_UNDEFINED;
	nx_ctx.exit_handler = JS_UNDEFINED;
	nx_ctx.error_handler = JS_UNDEFINED;
	nx_ctx.unhandled_rejection_handler = JS_UNDEFINED;
	nx_ctx.unhandled_rejected_promise = JS_UNDEFINED;
	JS_SetContextOpaque(ctx, &nx_ctx);

	// Register native canvas, dommatrix, font, and image modules
	nx_init_canvas(ctx, nx_ctx.init_obj);
	nx_init_dommatrix(ctx, nx_ctx.init_obj);
	nx_init_font(ctx, nx_ctx.init_obj);
	nx_init_image(ctx, nx_ctx.init_obj);

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

	// Set canvas dimensions as globals for the createCanvas helper
	JS_SetPropertyStr(ctx, global, "__canvas_width__",
	                  JS_NewInt32(ctx, canvas_width));
	JS_SetPropertyStr(ctx, global, "__canvas_height__",
	                  JS_NewInt32(ctx, canvas_height));

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

	// Provide createCanvas() using the runtime's OffscreenCanvas
	{
		char create_canvas_fn[512];
		snprintf(create_canvas_fn, sizeof(create_canvas_fn),
		         "globalThis.createCanvas = function(w, h) {\n"
		         "    if (w === undefined) w = %d;\n"
		         "    if (h === undefined) h = %d;\n"
		         "    var canvas = new OffscreenCanvas(w, h);\n"
		         "    var ctx = canvas.getContext('2d');\n"
		         "    globalThis.__nxjs_surface__ = canvas;\n"
		         "    return { canvas: canvas, ctx: ctx };\n"
		         "};\n",
		         canvas_width, canvas_height);
		JSValue cc_val =
		    JS_Eval(ctx, create_canvas_fn, strlen(create_canvas_fn),
		            "<createCanvas>", JS_EVAL_TYPE_GLOBAL);
		if (JS_IsException(cc_val)) {
			fprintf(stderr, "createCanvas setup failed:\n");
			print_js_error(ctx);
			JS_FreeValue(ctx, cc_val);
			JS_FreeContext(ctx);
			JS_FreeRuntime(rt);
			return 1;
		}
		JS_FreeValue(ctx, cc_val);
	}

	// Load and evaluate the test fixture
	if (eval_file(ctx, fixture_path, "fixture") != 0) {
		JS_FreeContext(ctx);
		JS_FreeRuntime(rt);
		return 1;
	}

	// Get the canvas surface and write to PNG
	cairo_surface_t *surface = get_canvas_surface(ctx);
	if (!surface) {
		fprintf(stderr, "No canvas surface found — did the fixture call "
		                "createCanvas()?\n");
		JS_FreeContext(ctx);
		JS_FreeRuntime(rt);
		return 1;
	}

	cairo_surface_flush(surface);
	cairo_status_t status = cairo_surface_write_to_png(surface, output_path);
	if (status != CAIRO_STATUS_SUCCESS) {
		fprintf(stderr, "Failed to write PNG: %s\n",
		        cairo_status_to_string(status));
		JS_FreeContext(ctx);
		JS_FreeRuntime(rt);
		return 1;
	}

	// Cleanup
	JS_FreeValue(ctx, nx_ctx.init_obj);
	JS_FreeContext(ctx);
	JS_FreeRuntime(rt);

	return 0;
}
