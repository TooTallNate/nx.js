/**
 * Canvas 2D Test Host
 *
 * Minimal QuickJS host for testing the nx.js Canvas 2D implementation
 * on x86_64 without a Nintendo Switch.
 *
 * Usage: nxjs-canvas-test <bridge.js> <fixture.js> <output.png> [width] [height]
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

int main(int argc, char *argv[]) {
	if (argc < 4) {
		fprintf(stderr,
		        "Usage: %s <bridge.js> <fixture.js> <output.png> [width] "
		        "[height]\n",
		        argv[0]);
		return 1;
	}

	const char *bridge_path = argv[1];
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

	// Expose init_obj as global '$' so the JS bridge can access native
	// functions
	JSValue global = JS_GetGlobalObject(ctx);
	JS_SetPropertyStr(ctx, global, "$", JS_DupValue(ctx, nx_ctx.init_obj));

	// Set canvas dimensions as globals for the bridge
	JS_SetPropertyStr(ctx, global, "__canvas_width__",
	                  JS_NewInt32(ctx, canvas_width));
	JS_SetPropertyStr(ctx, global, "__canvas_height__",
	                  JS_NewInt32(ctx, canvas_height));

	JS_FreeValue(ctx, global);

	// Load and evaluate the JS bridge
	size_t bridge_len;
	char *bridge_src = read_file(bridge_path, &bridge_len);
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
	char *fixture_src = read_file(fixture_path, &fixture_len);
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
