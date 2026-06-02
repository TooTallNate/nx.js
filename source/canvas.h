#pragma once
#include "types.h"
#include "font.h"
#include <cairo.h>
#include <harfbuzz/hb.h>

/**
 * `Screen` / `OffscreenCanvas` / `Image` / `ImageBitmap`
 */
typedef struct nx_canvas_s {
	uint32_t width;
	uint32_t height;
	uint8_t *data;
	cairo_surface_t *surface;
	bool surface_dirty;
} nx_canvas_t;

nx_canvas_t *nx_get_canvas(v8::Isolate *iso, v8::Local<v8::Value> obj);

// Accessors used by main.cc's framebuffer present path.
uint8_t *nx_canvas_pixels(nx_canvas_t *c);
uint32_t nx_canvas_width(nx_canvas_t *c);
uint32_t nx_canvas_height(nx_canvas_t *c);

typedef struct nx_rgba_s {
	double r, g, b, a;
} nx_rgba_t;

typedef enum {
	TEXT_BASELINE_ALPHABETIC,
	TEXT_BASELINE_TOP,
	TEXT_BASELINE_BOTTOM,
	TEXT_BASELINE_MIDDLE,
	TEXT_BASELINE_IDEOGRAPHIC,
	TEXT_BASELINE_HANGING
} text_baseline_t;

typedef enum {
	SOURCE_RGBA,
	SOURCE_GRADIENT,
	SOURCE_PATTERN,
} source_type_t;

typedef enum {
	TEXT_ALIGN_LEFT,
	TEXT_ALIGN_CENTER,
	TEXT_ALIGN_RIGHT,
	TEXT_ALIGN_START,
	TEXT_ALIGN_END
} text_align_t;

typedef struct nx_canvas_context_2d_state_s {
	nx_rgba_t fill;
	nx_rgba_t stroke;
	source_type_t fill_source_type;
	source_type_t stroke_source_type;
	cairo_pattern_t *fill_gradient;
	cairo_pattern_t *stroke_gradient;
	cairo_filter_t image_smoothing_quality;
	// The font face currently selected (replaces the old `JSValue font`; the
	// native font face pointer is sufficient for save/restore to re-select the
	// cairo font without holding a JS handle).
	nx_font_face_t *font_face;
	double font_size;
	const char *font_string;
	text_baseline_t text_baseline;
	text_align_t text_align;
	FT_Face ft_face;
	hb_font_t *hb_font;
	bool image_smoothing_enabled;
	double global_alpha;
	nx_rgba_t shadow_color;
	double shadow_blur;
	double shadow_offset_x;
	double shadow_offset_y;
	struct nx_canvas_context_2d_state_s *next;
} nx_canvas_context_2d_state_t;

typedef struct nx_canvas_context_2d_s {
	nx_canvas_t *canvas;
	cairo_t *ctx;
	cairo_path_t *path;
	nx_canvas_context_2d_state_t *state;
	nx_font_face_t *default_font_face;
} nx_canvas_context_2d_t;

nx_canvas_context_2d_t *nx_get_canvas_context_2d(v8::Isolate *iso,
                                                 v8::Local<v8::Value> obj);

void nx_init_canvas(v8::Isolate *iso, v8::Local<v8::Object> init_obj);
