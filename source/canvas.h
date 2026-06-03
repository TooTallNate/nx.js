#pragma once
#include "types.h"
#include "font.h"
#include <harfbuzz/hb.h>

#include "include/core/SkBlendMode.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkMatrix.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkShader.h"
#include "include/core/SkSurface.h"

#include <vector>

/**
 * `Screen` / `OffscreenCanvas` / `Image` / `ImageBitmap`
 *
 * Phase 2.1: raster-backed Skia. `data` is the backing pixel buffer
 * (kBGRA_8888 / premultiplied, byte layout identical to the former
 * CAIRO_FORMAT_ARGB32), wrapped by a raster `SkSurface` via MakeRasterDirect.
 * Keeping `data` lets the screen present path stay a plain memcpy and
 * get/putImageData stay cheap CPU ops. GPU backing is a Phase 2.2 follow-up.
 */
typedef struct nx_canvas_s {
	uint32_t width;
	uint32_t height;
	uint8_t *data;
	sk_sp<SkSurface> surface;
	bool surface_dirty;
	// Phase 2.2: when true, `surface` is a GPU-backed SkSurface (the screen's
	// EGL FBO 0) rather than a raster surface over `data`. getImageData/encode
	// read it back via SkSurface::readPixels; ensure_surface must not recreate
	// it as raster. `data` may be null in this mode.
	bool gpu;
} nx_canvas_t;

nx_canvas_t *nx_get_canvas(v8::Isolate *iso, v8::Local<v8::Value> obj);

// Accessors used by main.cc's framebuffer present path.
uint8_t *nx_canvas_pixels(nx_canvas_t *c);
uint32_t nx_canvas_width(nx_canvas_t *c);
uint32_t nx_canvas_height(nx_canvas_t *c);

// Phase 2.2: make `c` GPU-backed by adopting `surface` (the screen's EGL FBO 0
// SkSurface). The canvas's raster `data`/surface are released; subsequent draws
// target the GPU surface and getImageData reads it back. Used by main.cc for
// the screen canvas only.
void nx_canvas_set_gpu_surface(nx_canvas_t *c, sk_sp<SkSurface> surface);

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

// Image-smoothing quality, decoupled from Cairo's filter enum. Mapped to
// SkSamplingOptions at draw time.
typedef enum {
	IMAGE_SMOOTHING_LOW,
	IMAGE_SMOOTHING_MEDIUM,
	IMAGE_SMOOTHING_HIGH,
} image_smoothing_quality_t;

// A canvas gradient. Skia builds a shader from all stops at once (unlike Cairo's
// incremental add), so stops are buffered here and the shader is (re)built
// lazily when the gradient is used as a paint source. Reference-counted via the
// JS wrapper's finalizer; copied by value into save()'d states (the stop vector
// is cheap to copy and the built shader is an sk_sp).
typedef struct nx_canvas_gradient_s {
	bool radial;
	// linear: x0,y0 -> x1,y1. radial: (x0,y0,r0) -> (x1,y1,r1).
	double x0, y0, r0, x1, y1, r1;
	std::vector<double> stops;  // flat [offset, r, g, b, a] * n (r/g/b in 0..1)
	sk_sp<SkShader> shader;     // built lazily; invalidated when a stop is added
} nx_canvas_gradient_t;

typedef struct nx_canvas_context_2d_state_s {
	nx_rgba_t fill;
	nx_rgba_t stroke;
	source_type_t fill_source_type;
	source_type_t stroke_source_type;
	// Gradient sources. Owning pointers into the JS-wrapped gradient objects;
	// ref/unref is managed by save()/restore() (see canvas.cc). These are the
	// same objects the JS side holds, kept alive by the wrapper.
	nx_canvas_gradient_t *fill_gradient;
	nx_canvas_gradient_t *stroke_gradient;
	image_smoothing_quality_t image_smoothing_quality;
	// The font face currently selected. The native font face pointer is
	// sufficient for save/restore to re-select the font without a JS handle.
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

	// --- State that Cairo tracked implicitly on its context, but which
	// SkCanvas::save/restore does NOT cover (it only saves matrix + clip). We
	// must carry it on the state stack ourselves and re-derive the SkPaints.
	double line_width;
	SkPaint::Cap line_cap;
	SkPaint::Join line_join;
	double miter_limit;
	std::vector<float> line_dash;  // even count; CSS dash pattern
	double line_dash_offset;
	SkBlendMode blend_mode;
	SkPathFillType fill_rule;

	struct nx_canvas_context_2d_state_s *next;
} nx_canvas_context_2d_state_t;

typedef struct nx_canvas_context_2d_s {
	nx_canvas_t *canvas;
	SkCanvas *ctx;       // borrowed from canvas->surface (not owned)
	SkPathBuilder path;  // current path, built incrementally (snapshot to draw)
	nx_canvas_context_2d_state_t *state;
	nx_font_face_t *default_font_face;
} nx_canvas_context_2d_t;

nx_canvas_context_2d_t *nx_get_canvas_context_2d(v8::Isolate *iso,
                                                 v8::Local<v8::Value> obj);

void nx_init_canvas(v8::Isolate *iso, v8::Local<v8::Object> init_obj);
