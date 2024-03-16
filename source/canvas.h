#pragma once
#include <cairo.h>
#include <harfbuzz/hb.h>
#include "types.h"

/**
 * `Screen` / `OffscreenCanvas` / `Image` / `ImageBitmap`
 */
typedef struct nx_canvas_s
{
	uint32_t width;
	uint32_t height;
	uint8_t *data;
	size_t data_size;
	cairo_surface_t *surface;
} nx_canvas_t;

typedef struct nx_rgba_s
{
	double r;
	double g;
	double b;
	double a;
} nx_rgba_t;

typedef enum
{
	TEXT_BASELINE_ALPHABETIC,
	TEXT_BASELINE_TOP,
	TEXT_BASELINE_BOTTOM,
	TEXT_BASELINE_MIDDLE,
	TEXT_BASELINE_IDEOGRAPHIC,
	TEXT_BASELINE_HANGING
} text_baseline_t;

typedef enum
{
	TEXT_ALIGN_LEFT,
	TEXT_ALIGN_CENTER,
	TEXT_ALIGN_RIGHT,
	TEXT_ALIGN_START,
	TEXT_ALIGN_END
} text_align_t;

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
	cairo_filter_t image_smoothing_quality;
	JSValue font;
	double font_size;
	const char *font_string;
	text_baseline_t text_baseline;
	text_align_t text_align;
	FT_Face ft_face;
	hb_font_t *hb_font;
	bool image_smoothing_enabled;
	double global_alpha;
	struct nx_canvas_context_2d_state_s *next;
} nx_canvas_context_2d_state_t;

/**
 * `CanvasRenderingContext2D` / `OffscreenCanvasRenderingContext2D`
 */
typedef struct nx_canvas_context_2d_s
{
	nx_canvas_t *canvas;
	cairo_t *ctx;
	cairo_path_t *path;
	nx_canvas_context_2d_state_t *state;
} nx_canvas_context_2d_t;

nx_canvas_t *nx_get_canvas(JSContext *ctx, JSValueConst obj);
nx_canvas_context_2d_t *nx_get_canvas_context_2d(JSContext *ctx, JSValueConst obj);
int initialize_canvas(JSContext* ctx, nx_canvas_t* canvas, int width, int height);
int initialize_canvas_context_2d(JSContext *ctx, nx_canvas_context_2d_t *context);

void nx_init_canvas(JSContext *ctx, JSValueConst init_obj);
