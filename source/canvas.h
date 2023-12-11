#pragma once
#include <cairo.h>
#include <harfbuzz/hb.h>
#include "types.h"

/**
 * `Screen` / `OffscreenCanvas` / `Image` / `ImageBitmap`
 */
typedef struct
{
	uint32_t width;
	uint32_t height;
	uint8_t *data;
	cairo_surface_t *surface;
} nx_canvas_t;

nx_canvas_t *nx_get_canvas(JSContext *ctx, JSValueConst obj);

typedef struct
{
	double r;
	double g;
	double b;
	double a;
} nx_rgba_t;

/*
 * State struct.
 *
 * Used in conjunction with `ctx.save()` / `ctx.restore()` since
 * cairo's gstate maintains only a single source pattern at a time.
 */
typedef struct nx_canvas_context_2d_state_s
{

	nx_rgba_t fill;
	nx_rgba_t stroke;
	// nx_rgba_t shadow;
	// double shadowOffsetX;
	// double shadowOffsetY;
	// cairo_pattern_t *fillPattern;
	// cairo_pattern_t *strokePattern;
	// cairo_pattern_t *fillGradient;
	// cairo_pattern_t *strokeGradient;
	// int shadowBlur;
	// text_align_t textAlignment = TEXT_ALIGNMENT_LEFT; // TODO default is supposed to be START
	// text_baseline_t textBaseline = TEXT_BASELINE_ALPHABETIC;
	// canvas_draw_mode_t textDrawingMode;
	cairo_filter_t image_smoothing_quality;
	JSValue font;
	double font_size;
	const char *font_string;
	hb_font_t *hb_font;
	bool image_smoothing_enabled;
	double global_alpha;
	struct nx_canvas_context_2d_state_s *next;
} nx_canvas_context_2d_state_t;

/**
 * `CanvasRenderingContext2D` / `OffscreenCanvasRenderingContext2D`
 */
typedef struct
{
	nx_canvas_t *canvas;
	cairo_t *ctx;
	cairo_path_t *path;
	nx_canvas_context_2d_state_t *state;
} nx_canvas_context_2d_t;

nx_canvas_context_2d_t *nx_get_canvas_context_2d(JSContext *ctx, JSValueConst obj);

void nx_init_canvas(JSContext *ctx, JSValueConst init_obj);
