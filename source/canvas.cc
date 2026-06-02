/**
 * Canvas 2D — V8 binding shell over the Cairo rendering body (Phase 1).
 * Original Cairo logic from node-canvas (MIT). Only the JS boundary is V8;
 * all cairo/harfbuzz drawing is preserved verbatim.
 */
#include "canvas.h"
#include "async.h"
#include "dommatrix.h"
#include "error.h"
#include "font.h"
#include "image.h"
#include "util.h"
#include "wrap.h"
#include <alloca.h>
#include <math.h>
#include <mbedtls/base64.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <turbojpeg.h>
#include <webp/encode.h>

using namespace v8;

namespace {

typedef struct Point {
	float x, y;
} Point;

static inline int imin(int a, int b) { return a < b ? a : b; }
static inline float minf(float a, float b) { return a < b ? a : b; }
static inline void point_swap(Point *a, Point *b) {
	Point t = *a;
	*a = *b;
	*b = t;
}

static void set_font_size(nx_canvas_context_2d_t *context, double font_size);

// ---------------------------------------------------------------------------
// ensure_surface: recreate the cairo surface after a resize (Cairo body kept).
// ---------------------------------------------------------------------------
static void nx_canvas_ensure_surface(Isolate *iso,
                                     nx_canvas_context_2d_t *context) {
	nx_canvas_t *canvas = context->canvas;
	if (!canvas->surface_dirty)
		return;

	if (context->ctx) {
		cairo_destroy(context->ctx);
		context->ctx = NULL;
	}
	if (canvas->surface) {
		cairo_surface_finish(canvas->surface);
		cairo_surface_destroy(canvas->surface);
		canvas->surface = NULL;
	}
	if (canvas->data) {
		free(canvas->data);
		canvas->data = NULL;
	}

	if (canvas->width == 0 || canvas->height == 0) {
		canvas->surface_dirty = false;
		goto reset_state;
	}

	if (canvas->width > SIZE_MAX / 4 ||
	    (size_t)canvas->height > SIZE_MAX / ((size_t)canvas->width * 4)) {
		iso->ThrowException(
		    Exception::RangeError(nx_str(iso, "Canvas dimensions too large")));
		return;
	}

	{
		size_t buf_size = (size_t)canvas->width * canvas->height * 4;
		uint8_t *buffer = (uint8_t *)calloc(1, buf_size);
		if (!buffer) {
			nx_throw(iso, "out of memory");
			return;
		}
		canvas->data = buffer;
		canvas->surface = cairo_image_surface_create_for_data(
		    buffer, CAIRO_FORMAT_ARGB32, canvas->width, canvas->height,
		    canvas->width * 4);
		cairo_status_t surface_status = cairo_surface_status(canvas->surface);
		if (surface_status != CAIRO_STATUS_SUCCESS) {
			nx_throw(iso, "Failed to create Cairo surface");
			cairo_surface_destroy(canvas->surface);
			canvas->surface = NULL;
			free(canvas->data);
			canvas->data = NULL;
			return;
		}
		context->ctx = cairo_create(canvas->surface);
		cairo_status_t ctx_status = cairo_status(context->ctx);
		if (ctx_status != CAIRO_STATUS_SUCCESS) {
			nx_throw(iso, "Failed to create Cairo context");
			cairo_destroy(context->ctx);
			context->ctx = NULL;
			cairo_surface_destroy(canvas->surface);
			canvas->surface = NULL;
			free(canvas->data);
			canvas->data = NULL;
			return;
		}
		canvas->surface_dirty = false;
	}

reset_state:
	if (context->path) {
		cairo_path_destroy(context->path);
		context->path = NULL;
	}
	nx_canvas_context_2d_state_t *state = context->state;
	if (state->next) {
		nx_canvas_context_2d_state_t *s = state->next;
		while (s) {
			nx_canvas_context_2d_state_t *next = s->next;
			if (s->font_string)
				free((void *)s->font_string);
			if (s->fill_gradient)
				cairo_pattern_destroy(s->fill_gradient);
			if (s->stroke_gradient)
				cairo_pattern_destroy(s->stroke_gradient);
			free(s);
			s = next;
		}
		state->next = NULL;
	}
	if (state->font_string) {
		free((void *)state->font_string);
		state->font_string = NULL;
	}
	if (state->fill_gradient) {
		cairo_pattern_destroy(state->fill_gradient);
		state->fill_gradient = NULL;
	}
	if (state->stroke_gradient) {
		cairo_pattern_destroy(state->stroke_gradient);
		state->stroke_gradient = NULL;
	}
	state->font_face = NULL;
	state->font_size = 10.;
	state->ft_face = NULL;
	state->hb_font = NULL;
	state->fill = (nx_rgba_t){0., 0., 0., 1.};
	state->stroke = (nx_rgba_t){0., 0., 0., 1.};
	state->fill_source_type = SOURCE_RGBA;
	state->stroke_source_type = SOURCE_RGBA;
	state->global_alpha = 1.;
	state->image_smoothing_quality = CAIRO_FILTER_FAST;
	state->image_smoothing_enabled = true;
	state->text_align = TEXT_ALIGN_START;
	state->text_baseline = TEXT_BASELINE_ALPHABETIC;
	if (context->ctx)
		cairo_set_line_width(context->ctx, 1.);
	if (context->default_font_face) {
		nx_font_face_t *face = context->default_font_face;
		state->font_face = face;
		state->ft_face = face->ft_face;
		state->hb_font = face->hb_font;
		state->font_string = strdup("10px sans-serif");
		if (context->ctx) {
			cairo_set_font_face(context->ctx, face->cairo_font);
			set_font_size(context, 10.);
		}
	}
}

// ---------------------------------------------------------------------------
// Per-call context setup. Replaces CANVAS_CONTEXT_THIS / CANVAS_CONTEXT_ARGV0.
// Returns the context (ensuring the surface) or nullptr (with `cr` NULL) if it
// is a no-op (zero dims) or an exception was thrown.
// ---------------------------------------------------------------------------
struct Ctx2D {
	nx_canvas_context_2d_t *context;
	cairo_t *cr;
	bool ok;     // surface ready, proceed
	bool noop;   // zero-dim canvas: return without drawing, no exception
};

Ctx2D enter_ctx(Isolate *iso, Local<Value> recv) {
	Ctx2D r = {nullptr, nullptr, false, false};
	r.context = nx::Unwrap<nx_canvas_context_2d_t>(recv);
	if (!r.context)
		return r; // exception not thrown by caller; treat as error
	nx_canvas_ensure_surface(iso, r.context);
	if (!r.context->ctx) {
		if (r.context->canvas->width == 0 || r.context->canvas->height == 0)
			r.noop = true;
		return r;
	}
	r.cr = r.context->ctx;
	r.ok = true;
	return r;
}

// Read `n` doubles from info[offset..]. Returns false (throwing) on failure.
bool get_doubles(const FunctionCallbackInfo<Value> &info, double *args,
                 int n, int offset) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	for (int i = 0; i < n; i++) {
		if (!info[offset + i]->NumberValue(ctx).To(&args[i])) {
			return false;
		}
	}
	return true;
}

// Apply the user's Path2D onto the context by calling back into JS
// ($.applyPath(this, path)). `recv` is the JS context-2d object.
void apply_path(Isolate *iso, Local<Value> recv, Local<Value> path) {
	Local<Context> ctx = iso->GetCurrentContext();
	Local<Object> init_obj = nx_ctx(iso)->init_obj.Get(iso);
	Local<Value> fn_v;
	if (!init_obj->Get(ctx, nx_str(iso, "applyPath")).ToLocal(&fn_v) ||
	    !fn_v->IsFunction())
		return;
	Local<Function> fn = fn_v.As<Function>();
	Local<Value> args[] = {recv, path};
	TryCatch tc(iso);
	Local<Value> ret;
	if (!fn->Call(ctx, Null(iso), 2, args).ToLocal(&ret)) { /* ignore */ }
}

void set_fill_rule_v(Isolate *iso, Local<Value> fill_rule, cairo_t *cr) {
	cairo_fill_rule_t rule = CAIRO_FILL_RULE_WINDING;
	if (fill_rule->IsString()) {
		String::Utf8Value str(iso, fill_rule);
		if (*str && strcmp(*str, "evenodd") == 0)
			rule = CAIRO_FILL_RULE_EVEN_ODD;
	}
	cairo_set_fill_rule(cr, rule);
}

void save_path(nx_canvas_context_2d_t *context) {
	context->path = cairo_copy_path_flat(context->ctx);
	cairo_new_path(context->ctx);
}
void restore_path(nx_canvas_context_2d_t *context) {
	cairo_new_path(context->ctx);
	cairo_append_path(context->ctx, context->path);
	cairo_path_destroy(context->path);
	context->path = NULL;
}

static bool has_shadow(nx_canvas_context_2d_t *context) {
	return context->state->shadow_color.a > 0 &&
	       (context->state->shadow_blur > 0 ||
	        context->state->shadow_offset_x != 0 ||
	        context->state->shadow_offset_y != 0);
}

static void blur_surface(cairo_surface_t *surface, int radius) {
	if (radius <= 0)
		return;
	cairo_surface_flush(surface);
	unsigned char *data = cairo_image_surface_get_data(surface);
	int width = cairo_image_surface_get_width(surface);
	int height = cairo_image_surface_get_height(surface);
	int stride = cairo_image_surface_get_stride(surface);
	if (!data || width <= 0 || height <= 0)
		return;
	unsigned char *tmp = (unsigned char *)malloc(stride * height);
	if (!tmp)
		return;
	for (int pass = 0; pass < 3; pass++) {
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				int r = 0, g = 0, b = 0, a = 0, count = 0;
				for (int kx = -radius; kx <= radius; kx++) {
					int sx = x + kx;
					if (sx >= 0 && sx < width) {
						unsigned char *p = data + y * stride + sx * 4;
						b += p[0]; g += p[1]; r += p[2]; a += p[3];
						count++;
					}
				}
				unsigned char *d = tmp + y * stride + x * 4;
				d[0] = b / count; d[1] = g / count;
				d[2] = r / count; d[3] = a / count;
			}
		}
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				int r = 0, g = 0, b = 0, a = 0, count = 0;
				for (int ky = -radius; ky <= radius; ky++) {
					int sy = y + ky;
					if (sy >= 0 && sy < height) {
						unsigned char *p = tmp + sy * stride + x * 4;
						b += p[0]; g += p[1]; r += p[2]; a += p[3];
						count++;
					}
				}
				unsigned char *d = data + y * stride + x * 4;
				d[0] = b / count; d[1] = g / count;
				d[2] = r / count; d[3] = a / count;
			}
		}
	}
	free(tmp);
	cairo_surface_mark_dirty(surface);
}

typedef enum { SHADOW_DRAW_FILL, SHADOW_DRAW_STROKE } shadow_draw_mode_t;

static void apply_shadow(nx_canvas_context_2d_t *context,
                         shadow_draw_mode_t mode) {
	if (!has_shadow(context))
		return;
	cairo_t *cr = context->ctx;
	nx_canvas_context_2d_state_t *state = context->state;
	double blur = state->shadow_blur;
	if (blur > 0) {
		double x1, y1, x2, y2;
		cairo_path_t *path = cairo_copy_path(cr);
		if (mode == SHADOW_DRAW_FILL)
			cairo_fill_extents(cr, &x1, &y1, &x2, &y2);
		else
			cairo_stroke_extents(cr, &x1, &y1, &x2, &y2);
		int pad = (int)(blur * 2);
		int sw = (int)(x2 - x1) + pad * 2 + 2;
		int sh = (int)(y2 - y1) + pad * 2 + 2;
		if (sw <= 0 || sh <= 0) {
			cairo_path_destroy(path);
			return;
		}
		cairo_surface_t *shadow_surface =
		    cairo_image_surface_create(CAIRO_FORMAT_ARGB32, sw, sh);
		cairo_t *shadow_cr = cairo_create(shadow_surface);
		cairo_matrix_t matrix;
		cairo_get_matrix(cr, &matrix);
		cairo_set_matrix(shadow_cr, &matrix);
		cairo_translate(shadow_cr, -x1 + pad, -y1 + pad);
		cairo_set_line_width(shadow_cr, cairo_get_line_width(cr));
		cairo_set_line_cap(shadow_cr, cairo_get_line_cap(cr));
		cairo_set_line_join(shadow_cr, cairo_get_line_join(cr));
		cairo_set_miter_limit(shadow_cr, cairo_get_miter_limit(cr));
		cairo_new_path(shadow_cr);
		cairo_append_path(shadow_cr, path);
		cairo_set_source_rgba(shadow_cr, state->shadow_color.r,
		                      state->shadow_color.g, state->shadow_color.b,
		                      state->shadow_color.a * state->global_alpha);
		if (mode == SHADOW_DRAW_FILL)
			cairo_fill(shadow_cr);
		else
			cairo_stroke(shadow_cr);
		cairo_destroy(shadow_cr);
		blur_surface(shadow_surface, (int)(blur / 2));
		cairo_save(cr);
		cairo_identity_matrix(cr);
		cairo_set_source_surface(cr, shadow_surface,
		                         x1 - pad + state->shadow_offset_x,
		                         y1 - pad + state->shadow_offset_y);
		cairo_paint(cr);
		cairo_restore(cr);
		cairo_surface_destroy(shadow_surface);
		cairo_new_path(cr);
		cairo_append_path(cr, path);
		cairo_path_destroy(path);
	} else {
		cairo_path_t *path = cairo_copy_path(cr);
		cairo_save(cr);
		cairo_translate(cr, state->shadow_offset_x, state->shadow_offset_y);
		cairo_new_path(cr);
		cairo_append_path(cr, path);
		cairo_set_source_rgba(cr, state->shadow_color.r, state->shadow_color.g,
		                      state->shadow_color.b,
		                      state->shadow_color.a * state->global_alpha);
		if (mode == SHADOW_DRAW_FILL)
			cairo_fill(cr);
		else
			cairo_stroke(cr);
		cairo_restore(cr);
		cairo_new_path(cr);
		cairo_append_path(cr, path);
		cairo_path_destroy(path);
	}
}

static void fill_op(nx_canvas_context_2d_t *context, bool preserve) {
	apply_shadow(context, SHADOW_DRAW_FILL);
	if (context->state->fill_source_type == SOURCE_GRADIENT &&
	    context->state->fill_gradient) {
		cairo_set_source(context->ctx, context->state->fill_gradient);
	} else {
		cairo_set_source_rgba(context->ctx, context->state->fill.r,
		                      context->state->fill.g, context->state->fill.b,
		                      context->state->fill.a *
		                          context->state->global_alpha);
	}
	if (preserve)
		cairo_fill_preserve(context->ctx);
	else
		cairo_fill(context->ctx);
}

static void stroke_op(nx_canvas_context_2d_t *context, bool preserve) {
	apply_shadow(context, SHADOW_DRAW_STROKE);
	if (context->state->stroke_source_type == SOURCE_GRADIENT &&
	    context->state->stroke_gradient) {
		cairo_set_source(context->ctx, context->state->stroke_gradient);
	} else {
		cairo_set_source_rgba(context->ctx, context->state->stroke.r,
		                      context->state->stroke.g,
		                      context->state->stroke.b,
		                      context->state->stroke.a *
		                          context->state->global_alpha);
	}
	if (preserve)
		cairo_stroke_preserve(context->ctx);
	else
		cairo_stroke(context->ctx);
}

static void set_font_size(nx_canvas_context_2d_t *context, double font_size) {
	if (!context->state->ft_face || !context->state->hb_font)
		return;
	FT_Set_Char_Size(context->state->ft_face, 0, font_size * 64.0, 0, 0);
	cairo_set_font_size(context->ctx, font_size);
	hb_font_set_scale(context->state->hb_font, font_size * 64, font_size * 64);
}

double get_text_scale(nx_canvas_context_2d_t *context, const char *text,
                      double max_width) {
	if (!context->state->hb_font)
		return 1.;
	hb_buffer_t *buf = hb_buffer_create();
	hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
	hb_buffer_set_script(buf, HB_SCRIPT_COMMON);
	hb_buffer_set_language(buf, hb_language_get_default());
	hb_buffer_add_utf8(buf, text, -1, 0, -1);
	hb_shape(context->state->hb_font, buf, NULL, 0);
	unsigned int glyph_count = hb_buffer_get_length(buf);
	hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buf, NULL);
	double width = 0;
	for (unsigned int i = 0; i < glyph_count; ++i)
		width += glyph_pos[i].x_advance / (64.0);
	hb_buffer_destroy(buf);
	return width > max_width ? max_width / width : 1.;
}


// ===========================================================================
// Path / drawing methods.  `info.This()` is the CanvasRenderingContext2D.
// Helper macro pattern: enter the context, bail on no-op/exception.
// ===========================================================================

#define ENTER_THIS                                                             \
	Isolate *iso = info.GetIsolate();                                          \
	Ctx2D _c = enter_ctx(iso, info.This());                                    \
	if (!_c.ok) {                                                              \
		if (!_c.noop && !_c.context)                                           \
			nx_throw(iso, "invalid canvas context");                           \
		return;                                                                \
	}                                                                          \
	nx_canvas_context_2d_t *context = _c.context;                              \
	cairo_t *cr = _c.cr;                                                       \
	(void)context;                                                             \
	(void)cr;

// Variant entered via info[0] (the TS layer passes the context as arg 0 for
// some functions). Drawing args then start at offset 1.
#define ENTER_ARGV0                                                            \
	Isolate *iso = info.GetIsolate();                                          \
	Ctx2D _c = enter_ctx(iso, info[0]);                                        \
	if (!_c.ok) {                                                              \
		if (!_c.noop && !_c.context)                                           \
			nx_throw(iso, "invalid canvas context");                           \
		return;                                                                \
	}                                                                          \
	nx_canvas_context_2d_t *context = _c.context;                              \
	cairo_t *cr = _c.cr;                                                       \
	(void)context;                                                             \
	(void)cr;

void nx_canvas_context_2d_move_to(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double args[2];
	if (!get_doubles(info, args, 2, 0))
		return;
	cairo_move_to(cr, args[0], args[1]);
}

void nx_canvas_context_2d_line_to(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double args[2];
	if (!get_doubles(info, args, 2, 0))
		return;
	cairo_line_to(cr, args[0], args[1]);
}

void nx_canvas_context_2d_bezier_curve_to(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double args[6];
	if (!get_doubles(info, args, 6, 0))
		return;
	cairo_curve_to(cr, args[0], args[1], args[2], args[3], args[4], args[5]);
}

void nx_canvas_context_2d_quadratic_curve_to(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double args[4];
	if (!get_doubles(info, args, 4, 0))
		return;
	double x, y, x1 = args[0], y1 = args[1], x2 = args[2], y2 = args[3];
	cairo_get_current_point(cr, &x, &y);
	if (0 == x && 0 == y) {
		x = x1;
		y = y1;
	}
	cairo_curve_to(cr, x + 2.0 / 3.0 * (x1 - x), y + 2.0 / 3.0 * (y1 - y),
	               x2 + 2.0 / 3.0 * (x1 - x2), y2 + 2.0 / 3.0 * (y1 - y2), x2,
	               y2);
}

static double twoPi = M_PI * 2.;
static void canonicalizeAngle(double *startAngle, double *endAngle) {
	double newStartAngle = fmod(*startAngle, twoPi);
	if (newStartAngle < 0) {
		newStartAngle += twoPi;
		if (newStartAngle >= twoPi)
			newStartAngle -= twoPi;
	}
	double delta = newStartAngle - *startAngle;
	*startAngle = newStartAngle;
	*endAngle = *endAngle + delta;
}
static double adjustEndAngle(double startAngle, double endAngle,
                             int counterclockwise) {
	double newEndAngle = endAngle;
	if (!counterclockwise && endAngle - startAngle >= twoPi)
		newEndAngle = startAngle + twoPi;
	else if (counterclockwise && startAngle - endAngle >= twoPi)
		newEndAngle = startAngle - twoPi;
	else if (!counterclockwise && startAngle > endAngle)
		newEndAngle = startAngle + (twoPi - fmod(startAngle - endAngle, twoPi));
	else if (counterclockwise && startAngle < endAngle)
		newEndAngle = startAngle - (twoPi - fmod(endAngle - startAngle, twoPi));
	return newEndAngle;
}

void nx_canvas_context_2d_arc(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double args[5];
	if (!get_doubles(info, args, 5, 0))
		return;
	double x = args[0], y = args[1], radius = args[2], startAngle = args[3],
	       endAngle = args[4];
	if (radius < 0) {
		iso->ThrowException(
		    Exception::RangeError(nx_str(iso, "The radius provided is negative.")));
		return;
	}
	int counterclockwise =
	    info.Length() > 5 ? (info[5]->BooleanValue(iso) ? 1 : 0) : 0;
	canonicalizeAngle(&startAngle, &endAngle);
	endAngle = adjustEndAngle(startAngle, endAngle, counterclockwise);
	if (counterclockwise)
		cairo_arc_negative(cr, x, y, radius, startAngle, endAngle);
	else
		cairo_arc(cr, x, y, radius, startAngle, endAngle);
}

void nx_canvas_context_2d_arc_to(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double args[5];
	if (!get_doubles(info, args, 5, 0))
		return;
	double x, y;
	cairo_get_current_point(cr, &x, &y);
	Point p0 = {(float)x, (float)y};
	Point p1 = {(float)args[0], (float)args[1]};
	Point p2 = {(float)args[2], (float)args[3]};
	float radius = args[4];
	if ((p1.x == p0.x && p1.y == p0.y) || (p1.x == p2.x && p1.y == p2.y) ||
	    radius == 0.f) {
		cairo_line_to(cr, p1.x, p1.y);
		return;
	}
	Point p1p0 = {(p0.x - p1.x), (p0.y - p1.y)};
	Point p1p2 = {(p2.x - p1.x), (p2.y - p1.y)};
	float p1p0_length = sqrtf(p1p0.x * p1p0.x + p1p0.y * p1p0.y);
	float p1p2_length = sqrtf(p1p2.x * p1p2.x + p1p2.y * p1p2.y);
	double cos_phi =
	    (p1p0.x * p1p2.x + p1p0.y * p1p2.y) / (p1p0_length * p1p2_length);
	if (-1 == cos_phi) {
		cairo_line_to(cr, p1.x, p1.y);
		return;
	}
	if (1 == cos_phi) {
		unsigned int max_length = 65535;
		double factor_max = max_length / p1p0_length;
		Point ep = {(float)(p0.x + factor_max * p1p0.x),
		            (float)(p0.y + factor_max * p1p0.y)};
		cairo_line_to(cr, ep.x, ep.y);
		return;
	}
	float tangent = radius / tan(acos(cos_phi) / 2);
	float factor_p1p0 = tangent / p1p0_length;
	Point t_p1p0 = {(p1.x + factor_p1p0 * p1p0.x),
	                (p1.y + factor_p1p0 * p1p0.y)};
	Point orth_p1p0 = {p1p0.y, -p1p0.x};
	float orth_p1p0_length =
	    sqrt(orth_p1p0.x * orth_p1p0.x + orth_p1p0.y * orth_p1p0.y);
	float factor_ra = radius / orth_p1p0_length;
	double cos_alpha = (orth_p1p0.x * p1p2.x + orth_p1p0.y * p1p2.y) /
	                   (orth_p1p0_length * p1p2_length);
	if (cos_alpha < 0.f) {
		orth_p1p0.x = -orth_p1p0.x;
		orth_p1p0.y = -orth_p1p0.y;
	}
	Point p = {(t_p1p0.x + factor_ra * orth_p1p0.x),
	           (t_p1p0.y + factor_ra * orth_p1p0.y)};
	orth_p1p0.x = -orth_p1p0.x;
	orth_p1p0.y = -orth_p1p0.y;
	float sa = acos(orth_p1p0.x / orth_p1p0_length);
	if (orth_p1p0.y < 0.f)
		sa = 2 * M_PI - sa;
	int anticlockwise = 0;
	float factor_p1p2 = tangent / p1p2_length;
	Point t_p1p2 = {(p1.x + factor_p1p2 * p1p2.x),
	                (p1.y + factor_p1p2 * p1p2.y)};
	Point orth_p1p2 = {(t_p1p2.x - p.x), (t_p1p2.y - p.y)};
	float orth_p1p2_length =
	    sqrtf(orth_p1p2.x * orth_p1p2.x + orth_p1p2.y * orth_p1p2.y);
	float ea = acos(orth_p1p2.x / orth_p1p2_length);
	if (orth_p1p2.y < 0)
		ea = 2 * M_PI - ea;
	if ((sa > ea) && ((sa - ea) < M_PI))
		anticlockwise = 1;
	if ((sa < ea) && ((ea - sa) > M_PI))
		anticlockwise = 1;
	cairo_line_to(cr, t_p1p0.x, t_p1p0.y);
	if (anticlockwise && M_PI * 2 != radius)
		cairo_arc_negative(cr, p.x, p.y, radius, sa, ea);
	else
		cairo_arc(cr, p.x, p.y, radius, sa, ea);
}

void nx_canvas_context_2d_ellipse(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double args[7];
	if (!get_doubles(info, args, 7, 0))
		return;
	double radiusX = args[2], radiusY = args[3];
	if (radiusX == 0 || radiusY == 0)
		return;
	double x = args[0], y = args[1], rotation = args[4], startAngle = args[5],
	       endAngle = args[6];
	int anticlockwise =
	    info.Length() >= 8 ? (info[7]->BooleanValue(iso) ? 1 : 0) : 0;
	double xRatio = radiusX / radiusY;
	cairo_matrix_t save_matrix;
	cairo_get_matrix(cr, &save_matrix);
	cairo_translate(cr, x, y);
	cairo_rotate(cr, rotation);
	cairo_scale(cr, xRatio, 1.0);
	cairo_translate(cr, -x, -y);
	if (anticlockwise && M_PI * 2 != args[4])
		cairo_arc_negative(cr, x, y, radiusY, startAngle, endAngle);
	else
		cairo_arc(cr, x, y, radiusY, startAngle, endAngle);
	cairo_set_matrix(cr, &save_matrix);
}

void nx_canvas_context_2d_rect(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double args[4];
	if (!get_doubles(info, args, 4, 0))
		return;
	double x = args[0], y = args[1], width = args[2], height = args[3];
	if (width == 0) {
		cairo_move_to(cr, x, y);
		cairo_line_to(cr, x, y + height);
	} else if (height == 0) {
		cairo_move_to(cr, x, y);
		cairo_line_to(cr, x + width, y);
	} else {
		cairo_rectangle(cr, x, y, width, height);
	}
}

inline static void elli_arc(cairo_t *cr, double xc, double yc, double rx,
                            double ry, double a1, double a2, bool clockwise) {
	if (rx == 0. || ry == 0.) {
		cairo_line_to(cr, xc + rx, yc + ry);
	} else {
		cairo_save(cr);
		cairo_translate(cr, xc, yc);
		cairo_scale(cr, rx, ry);
		if (clockwise)
			cairo_arc(cr, 0., 0., 1., a1, a2);
		else
			cairo_arc_negative(cr, 0., 0., 1., a2, a1);
		cairo_restore(cr);
	}
}

// Returns true (throwing) on error.
inline static bool get_radius(Isolate *iso, Local<Value> v, Point *p) {
	Local<Context> ctx = iso->GetCurrentContext();
	if (v->IsObject()) {
		Local<Object> o = v.As<Object>();
		Local<Value> rx, ry;
		if (!o->Get(ctx, nx_str(iso, "x")).ToLocal(&rx) ||
		    !o->Get(ctx, nx_str(iso, "y")).ToLocal(&ry) || !rx->IsNumber() ||
		    !ry->IsNumber()) {
			nx_throw(iso, "A DOMPoint object must be provided");
			return true;
		}
		double rxv, ryv;
		if (!rx->NumberValue(ctx).To(&rxv) || !ry->NumberValue(ctx).To(&ryv))
			return true;
		if (!isfinite(rxv) || !isfinite(ryv))
			return true;
		if (rxv < 0 || ryv < 0) {
			iso->ThrowException(
			    Exception::RangeError(nx_str(iso, "radii must be positive.")));
			return true;
		}
		p->x = rxv;
		p->y = ryv;
		return false;
	} else if (v->IsNumber()) {
		double rv;
		if (!v->NumberValue(ctx).To(&rv))
			return true;
		if (!isfinite(rv))
			return true;
		if (rv < 0) {
			iso->ThrowException(
			    Exception::RangeError(nx_str(iso, "radii must be positive.")));
			return true;
		}
		p->x = p->y = rv;
		return false;
	}
	nx_throw(iso, "Unsupported radii value.");
	return true;
}

void nx_canvas_context_2d_round_rect(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	Local<Context> jsctx = iso->GetCurrentContext();
	double rargs[4];
	if (!get_doubles(info, rargs, 4, 0))
		return;
	double x = rargs[0], y = rargs[1], width = rargs[2], height = rargs[3];
	Point normalizedRadii[4];
	uint32_t nRadii = 4;
	if (info.Length() < 5 || info[4]->IsUndefined()) {
		for (int i = 0; i < 4; i++)
			normalizedRadii[i].x = normalizedRadii[i].y = 0.;
	} else if (info[4]->IsArray()) {
		Local<Object> arr = info[4].As<Object>();
		if (!arr->Get(jsctx, nx_str(iso, "length"))
		         .ToLocalChecked()
		         ->Uint32Value(jsctx)
		         .To(&nRadii))
			return;
		if (!(nRadii >= 1 && nRadii <= 4)) {
			iso->ThrowException(Exception::RangeError(nx_str(
			    iso, "radii must be a list of one, two, three or four radii.")));
			return;
		}
		for (uint32_t i = 0; i < nRadii; i++) {
			Local<Value> v = arr->Get(jsctx, i).ToLocalChecked();
			if (get_radius(iso, v, &normalizedRadii[i]))
				return;
		}
	} else {
		if (get_radius(iso, info[4], &normalizedRadii[0]))
			return;
		for (int i = 1; i < 4; i++) {
			normalizedRadii[i].x = normalizedRadii[0].x;
			normalizedRadii[i].y = normalizedRadii[0].y;
		}
	}
	Point upperLeft, upperRight, lowerRight, lowerLeft;
	if (nRadii == 4) {
		upperLeft = normalizedRadii[0];
		upperRight = normalizedRadii[1];
		lowerRight = normalizedRadii[2];
		lowerLeft = normalizedRadii[3];
	} else if (nRadii == 3) {
		upperLeft = normalizedRadii[0];
		upperRight = normalizedRadii[1];
		lowerLeft = normalizedRadii[1];
		lowerRight = normalizedRadii[2];
	} else if (nRadii == 2) {
		upperLeft = normalizedRadii[0];
		lowerRight = normalizedRadii[0];
		upperRight = normalizedRadii[1];
		lowerLeft = normalizedRadii[1];
	} else {
		upperLeft = upperRight = lowerRight = lowerLeft = normalizedRadii[0];
	}
	bool clockwise = true;
	if (width < 0) {
		clockwise = false;
		x += width;
		width = -width;
		point_swap(&upperLeft, &upperRight);
		point_swap(&lowerLeft, &lowerRight);
	}
	if (height < 0) {
		clockwise = !clockwise;
		y += height;
		height = -height;
		point_swap(&upperLeft, &upperRight);
		point_swap(&lowerLeft, &lowerRight);
	}
	{
		float top = upperLeft.x + upperRight.x;
		float right = upperRight.y + lowerRight.y;
		float bottom = lowerRight.x + lowerLeft.x;
		float left = upperLeft.y + lowerLeft.y;
		float scale = minf(width / top, minf(height / right,
		                                     minf(width / bottom,
		                                          height / left)));
		if (scale < 1.) {
			upperLeft.x *= scale; upperLeft.y *= scale;
			upperRight.x *= scale; upperRight.y *= scale;
			lowerLeft.x *= scale; lowerLeft.y *= scale;
			lowerRight.x *= scale; lowerRight.y *= scale;
		}
	}
	cairo_move_to(cr, x + upperLeft.x, y);
	if (clockwise) {
		cairo_line_to(cr, x + width - upperRight.x, y);
		elli_arc(cr, x + width - upperRight.x, y + upperRight.y, upperRight.x,
		         upperRight.y, 3. * M_PI / 2., 0., true);
		cairo_line_to(cr, x + width, y + height - lowerRight.y);
		elli_arc(cr, x + width - lowerRight.x, y + height - lowerRight.y,
		         lowerRight.x, lowerRight.y, 0, M_PI / 2., true);
		cairo_line_to(cr, x + lowerLeft.x, y + height);
		elli_arc(cr, x + lowerLeft.x, y + height - lowerLeft.y, lowerLeft.x,
		         lowerLeft.y, M_PI / 2., M_PI, true);
		cairo_line_to(cr, x, y + upperLeft.y);
		elli_arc(cr, x + upperLeft.x, y + upperLeft.y, upperLeft.x,
		         upperLeft.y, M_PI, 3. * M_PI / 2., true);
	} else {
		elli_arc(cr, x + upperLeft.x, y + upperLeft.y, upperLeft.x,
		         upperLeft.y, M_PI, 3. * M_PI / 2., false);
		cairo_line_to(cr, x, y + upperLeft.y);
		elli_arc(cr, x + lowerLeft.x, y + height - lowerLeft.y, lowerLeft.x,
		         lowerLeft.y, M_PI / 2., M_PI, false);
		cairo_line_to(cr, x + lowerLeft.x, y + height);
		elli_arc(cr, x + width - lowerRight.x, y + height - lowerRight.y,
		         lowerRight.x, lowerRight.y, 0, M_PI / 2., false);
		cairo_line_to(cr, x + width, y + height - lowerRight.y);
		elli_arc(cr, x + width - upperRight.x, y + upperRight.y, upperRight.x,
		         upperRight.y, 3. * M_PI / 2., 0., false);
		cairo_line_to(cr, x + width - upperRight.x, y);
	}
	cairo_close_path(cr);
}
// ===========================================================================
// Text rendering (HarfBuzz shaping -> cairo glyphs). Cairo body preserved.
// ===========================================================================

// Shared glyph layout helper used by fill_text / stroke_text.
static cairo_glyph_t *layout_glyphs(nx_canvas_context_2d_t *context,
                                    const char *text, double ox, double oy,
                                    unsigned int *out_count,
                                    hb_buffer_t **out_buf) {
	hb_buffer_t *buf = hb_buffer_create();
	hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
	hb_buffer_set_script(buf, HB_SCRIPT_COMMON);
	hb_buffer_set_language(buf, hb_language_get_default());
	hb_buffer_add_utf8(buf, text, -1, 0, -1);
	hb_shape(context->state->hb_font, buf, NULL, 0);
	unsigned int glyph_count = hb_buffer_get_length(buf);
	hb_glyph_info_t *gi = hb_buffer_get_glyph_infos(buf, NULL);
	hb_glyph_position_t *gp = hb_buffer_get_glyph_positions(buf, NULL);
	cairo_glyph_t *glyphs = cairo_glyph_allocate(glyph_count);
	double x = 0, y = 0;
	for (unsigned int i = 0; i < glyph_count; ++i) {
		glyphs[i].index = gi[i].codepoint;
		glyphs[i].x = x + (gp[i].x_offset / 64.0);
		glyphs[i].y = -(y + gp[i].y_offset / 64.0);
		x += gp[i].x_advance / 64.0;
		y += gp[i].y_advance / 64.0;
	}
	double alignment_offset = 0;
	if (context->state->text_align == TEXT_ALIGN_END ||
	    context->state->text_align == TEXT_ALIGN_RIGHT)
		alignment_offset = -x;
	else if (context->state->text_align == TEXT_ALIGN_CENTER)
		alignment_offset = -x / 2.0;
	double baseline_offset = 0;
	FT_Face ft = context->state->ft_face;
	if (context->state->text_baseline == TEXT_BASELINE_TOP)
		baseline_offset = ft->size->metrics.ascender / 64.0;
	else if (context->state->text_baseline == TEXT_BASELINE_HANGING)
		baseline_offset = (ft->size->metrics.ascender / 64.0) * 0.80;
	else if (context->state->text_baseline == TEXT_BASELINE_MIDDLE)
		baseline_offset = ((ft->size->metrics.ascender / 64.0) +
		                   (ft->size->metrics.descender / 64.0)) /
		                  2.0;
	else if (context->state->text_baseline == TEXT_BASELINE_IDEOGRAPHIC)
		baseline_offset = ft->size->metrics.descender / 64.0;
	else if (context->state->text_baseline == TEXT_BASELINE_BOTTOM)
		baseline_offset = (ft->size->metrics.descender / 64.0) * 2.0;
	for (unsigned int i = 0; i < glyph_count; ++i) {
		glyphs[i].x += ox + alignment_offset;
		glyphs[i].y += oy + baseline_offset;
	}
	*out_count = glyph_count;
	*out_buf = buf;
	return glyphs;
}

void nx_canvas_context_2d_fill_text(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	if (!context->state->hb_font || !context->state->ft_face)
		return;
	double args[2];
	if (!get_doubles(info, args, 2, 1))
		return;
	String::Utf8Value text(iso, info[0]);
	if (!*text)
		return;
	double scale = 1., font_size = context->state->font_size;
	if (info.Length() >= 4 && info[3]->IsNumber()) {
		double max_width;
		if (!info[3]->NumberValue(iso->GetCurrentContext()).To(&max_width))
			return;
		scale = get_text_scale(context, *text, max_width);
		if (scale != 1.)
			set_font_size(context, font_size * scale);
	}
	unsigned int glyph_count;
	hb_buffer_t *buf;
	cairo_glyph_t *glyphs =
	    layout_glyphs(context, *text, args[0], args[1], &glyph_count, &buf);
	if (context->state->fill_source_type == SOURCE_GRADIENT &&
	    context->state->fill_gradient)
		cairo_set_source(cr, context->state->fill_gradient);
	else
		cairo_set_source_rgba(cr, context->state->fill.r,
		                      context->state->fill.g, context->state->fill.b,
		                      context->state->fill.a *
		                          context->state->global_alpha);
	cairo_show_glyphs(cr, glyphs, glyph_count);
	if (scale != 1.)
		set_font_size(context, font_size);
	cairo_glyph_free(glyphs);
	hb_buffer_destroy(buf);
}

void nx_canvas_context_2d_stroke_text(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	if (!context->state->hb_font || !context->state->ft_face)
		return;
	double args[2];
	if (!get_doubles(info, args, 2, 1))
		return;
	String::Utf8Value text(iso, info[0]);
	if (!*text)
		return;
	save_path(context);
	double scale = 1., font_size = context->state->font_size;
	if (info.Length() >= 4 && info[3]->IsNumber()) {
		double max_width;
		if (!info[3]->NumberValue(iso->GetCurrentContext()).To(&max_width))
			return;
		scale = get_text_scale(context, *text, max_width);
		if (scale != 1.)
			set_font_size(context, font_size * scale);
	}
	unsigned int glyph_count;
	hb_buffer_t *buf;
	cairo_glyph_t *glyphs =
	    layout_glyphs(context, *text, args[0], args[1], &glyph_count, &buf);
	cairo_glyph_path(cr, glyphs, glyph_count);
	stroke_op(context, false);
	if (scale != 1.)
		set_font_size(context, font_size);
	restore_path(context);
	cairo_glyph_free(glyphs);
	hb_buffer_destroy(buf);
}

void nx_canvas_context_2d_measure_text(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	Local<Context> jsctx = iso->GetCurrentContext();
	Local<Object> metrics = Object::New(iso);
	auto set0 = [&](const char *k, double v) {
		metrics->Set(jsctx, nx_str(iso, k), Number::New(iso, v)).Check();
	};
	double width = 0;
	if (context->state->hb_font) {
		String::Utf8Value text(iso, info[0]);
		hb_buffer_t *buf = hb_buffer_create();
		hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
		hb_buffer_set_script(buf, HB_SCRIPT_COMMON);
		hb_buffer_set_language(buf, hb_language_get_default());
		hb_buffer_add_utf8(buf, *text ? *text : "", -1, 0, -1);
		hb_shape(context->state->hb_font, buf, NULL, 0);
		unsigned int glyph_count = hb_buffer_get_length(buf);
		hb_glyph_position_t *gp = hb_buffer_get_glyph_positions(buf, NULL);
		for (unsigned int i = 0; i < glyph_count; ++i)
			width += gp[i].x_advance / 64.0;
		hb_buffer_destroy(buf);
	}
	set0("width", width);
	set0("actualBoundingBoxLeft", 0);
	set0("actualBoundingBoxRight", 0);
	set0("fontBoundingBoxAscent", 0);
	set0("fontBoundingBoxDescent", 0);
	set0("actualBoundingBoxAscent", 0);
	set0("actualBoundingBoxDescent", 0);
	set0("emHeightAscent", 0);
	set0("emHeightDescent", 0);
	set0("hangingBaseline", 0);
	set0("alphabeticBaseline", 0);
	set0("ideographicBaseline", 0);
	info.GetReturnValue().Set(metrics);
}

// ===========================================================================
// font / transform / style getters+setters (ENTER_ARGV0 variants).
// ===========================================================================

void nx_canvas_context_2d_get_font(const FunctionCallbackInfo<Value> &info) {
	ENTER_ARGV0;
	info.GetReturnValue().Set(nx_str(
	    iso, context->state->font_string ? context->state->font_string : ""));
}

void nx_canvas_context_2d_set_font(const FunctionCallbackInfo<Value> &info) {
	ENTER_ARGV0;
	nx_font_face_t *face = nx_get_font_face(iso, info[1]);
	if (!face) {
		nx_throw(iso, "invalid font face");
		return;
	}
	double font_size;
	if (!info[2]->NumberValue(iso->GetCurrentContext()).To(&font_size))
		return;
	String::Utf8Value font_string(iso, info[3]);
	if (!*font_string)
		return;
	context->state->font_face = face;
	context->state->font_size = font_size;
	if (context->state->font_string)
		free((void *)context->state->font_string);
	context->state->font_string = strdup(*font_string);
	context->state->ft_face = face->ft_face;
	context->state->hb_font = face->hb_font;
	cairo_set_font_face(cr, face->cairo_font);
	set_font_size(context, font_size);
	if (!context->default_font_face)
		context->default_font_face = face;
}

void nx_canvas_context_2d_get_transform(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_ARGV0;
	Local<Context> jsctx = iso->GetCurrentContext();
	cairo_matrix_t matrix;
	cairo_get_matrix(cr, &matrix);
	Local<Array> array = Array::New(iso, 6);
	double v[6] = {matrix.xx, matrix.yx, matrix.xy,
	               matrix.yy, matrix.x0, matrix.y0};
	for (int i = 0; i < 6; i++)
		array->Set(jsctx, i, Number::New(iso, v[i])).Check();
	info.GetReturnValue().Set(array);
}

void nx_canvas_context_2d_get_fill_style(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_ARGV0;
	Local<Context> jsctx = iso->GetCurrentContext();
	Local<Array> rgba = Array::New(iso, 4);
	rgba->Set(jsctx, 0, Integer::New(iso, context->state->fill.r * 255)).Check();
	rgba->Set(jsctx, 1, Integer::New(iso, context->state->fill.g * 255)).Check();
	rgba->Set(jsctx, 2, Integer::New(iso, context->state->fill.b * 255)).Check();
	rgba->Set(jsctx, 3, Number::New(iso, context->state->fill.a)).Check();
	info.GetReturnValue().Set(rgba);
}

void nx_canvas_context_2d_set_fill_style(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_ARGV0;
	double args[4];
	if (!get_doubles(info, args, 4, 1))
		return;
	context->state->fill.r = args[0] / 255.;
	context->state->fill.g = args[1] / 255.;
	context->state->fill.b = args[2] / 255.;
	context->state->fill.a = args[3];
	context->state->fill_source_type = SOURCE_RGBA;
	if (context->state->fill_gradient) {
		cairo_pattern_destroy(context->state->fill_gradient);
		context->state->fill_gradient = NULL;
	}
}

void nx_canvas_context_2d_get_stroke_style(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_ARGV0;
	Local<Context> jsctx = iso->GetCurrentContext();
	Local<Array> rgba = Array::New(iso, 4);
	rgba->Set(jsctx, 0, Integer::New(iso, context->state->stroke.r * 255)).Check();
	rgba->Set(jsctx, 1, Integer::New(iso, context->state->stroke.g * 255)).Check();
	rgba->Set(jsctx, 2, Integer::New(iso, context->state->stroke.b * 255)).Check();
	rgba->Set(jsctx, 3, Number::New(iso, context->state->stroke.a)).Check();
	info.GetReturnValue().Set(rgba);
}

void nx_canvas_context_2d_set_stroke_style(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_ARGV0;
	double args[4];
	if (!get_doubles(info, args, 4, 1))
		return;
	context->state->stroke.r = args[0] / 255.;
	context->state->stroke.g = args[1] / 255.;
	context->state->stroke.b = args[2] / 255.;
	context->state->stroke.a = args[3];
	context->state->stroke_source_type = SOURCE_RGBA;
	if (context->state->stroke_gradient) {
		cairo_pattern_destroy(context->state->stroke_gradient);
		context->state->stroke_gradient = NULL;
	}
}

// ===========================================================================
// rect ops, path ops, state, line/shadow/composite/smoothing/text props.
// ===========================================================================

void nx_canvas_context_2d_stroke_rect(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double a[4];
	if (!get_doubles(info, a, 4, 0))
		return;
	if (a[2] && a[3]) {
		save_path(context);
		cairo_rectangle(cr, a[0], a[1], a[2], a[3]);
		stroke_op(context, false);
		restore_path(context);
	}
}

void nx_canvas_context_2d_clear_rect(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double a[4];
	if (!get_doubles(info, a, 4, 0))
		return;
	if (a[2] && a[3]) {
		cairo_save(cr);
		save_path(context);
		cairo_rectangle(cr, a[0], a[1], a[2], a[3]);
		cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
		cairo_fill(cr);
		restore_path(context);
		cairo_restore(cr);
	}
}

void nx_canvas_context_2d_fill_rect(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double a[4];
	if (!get_doubles(info, a, 4, 0))
		return;
	if (a[2] && a[3]) {
		save_path(context);
		cairo_rectangle(cr, a[0], a[1], a[2], a[3]);
		fill_op(context, false);
		restore_path(context);
	}
}

void nx_canvas_context_2d_begin_path(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	cairo_new_path(cr);
}
void nx_canvas_context_2d_close_path(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	cairo_close_path(cr);
}
void nx_canvas_context_2d_clip(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	set_fill_rule_v(iso, info[0], cr);
	cairo_clip_preserve(cr);
}

void nx_canvas_context_2d_fill(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	Local<Value> path, fill_rule;
	bool have_path = false, have_rule = false;
	if (info.Length() == 1) {
		if (info[0]->IsObject()) {
			path = info[0];
			have_path = true;
		} else if (info[0]->IsString()) {
			fill_rule = info[0];
			have_rule = true;
		} else if (!info[0]->IsUndefined()) {
			nx_throw(iso, "Expected Path2D or string at index 0");
			return;
		}
	} else if (info.Length() == 2) {
		if (info[0]->IsObject()) {
			path = info[0];
			have_path = true;
		} else {
			nx_throw(iso, "Expected Path2D at index 0");
			return;
		}
		if (info[1]->IsString()) {
			fill_rule = info[1];
			have_rule = true;
		} else if (!info[1]->IsUndefined()) {
			nx_throw(iso, "Expected string at index 1");
			return;
		}
	}
	if (have_rule)
		set_fill_rule_v(iso, fill_rule, cr);
	if (!have_path) {
		fill_op(context, true);
	} else {
		save_path(context);
		apply_path(iso, info.This(), path);
		fill_op(context, false);
		restore_path(context);
	}
}

void nx_canvas_context_2d_stroke(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	Local<Value> path;
	bool have_path = false;
	if (info.Length() == 1) {
		if (info[0]->IsObject()) {
			path = info[0];
			have_path = true;
		} else {
			nx_throw(iso, "Expected Path2D at index 0");
			return;
		}
	}
	if (!have_path) {
		stroke_op(context, true);
	} else {
		save_path(context);
		apply_path(iso, info.This(), path);
		stroke_op(context, false);
		restore_path(context);
	}
}

void nx_canvas_context_2d_save(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	cairo_save(cr);
	nx_canvas_context_2d_state_t *state =
	    (nx_canvas_context_2d_state_t *)calloc(
	        1, sizeof(nx_canvas_context_2d_state_t));
	memcpy(state, context->state, sizeof(nx_canvas_context_2d_state_t));
	state->next = context->state;
	if (context->state->font_string)
		state->font_string = strdup(context->state->font_string);
	if (state->fill_gradient)
		cairo_pattern_reference(state->fill_gradient);
	if (state->stroke_gradient)
		cairo_pattern_reference(state->stroke_gradient);
	context->state = state;
}

void nx_canvas_context_2d_restore(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	if (context->state->next) {
		cairo_restore(cr);
		nx_canvas_context_2d_state_t *prev = context->state;
		context->state = prev->next;
		if (prev->font_string)
			free((void *)prev->font_string);
		if (prev->fill_gradient)
			cairo_pattern_destroy(prev->fill_gradient);
		if (prev->stroke_gradient)
			cairo_pattern_destroy(prev->stroke_gradient);
		free(prev);
		nx_font_face_t *face = context->state->font_face;
		if (face) {
			cairo_set_font_face(cr, face->cairo_font);
			set_font_size(context, context->state->font_size);
		}
	}
}

void nx_canvas_context_2d_get_line_width(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	info.GetReturnValue().Set(Number::New(iso, cairo_get_line_width(cr)));
}
void nx_canvas_context_2d_set_line_width(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double n;
	if (!info[0]->NumberValue(iso->GetCurrentContext()).To(&n))
		return;
	cairo_set_line_width(cr, n);
}

void nx_canvas_context_2d_get_line_join(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	const char *join;
	switch (cairo_get_line_join(cr)) {
	case CAIRO_LINE_JOIN_BEVEL: join = "bevel"; break;
	case CAIRO_LINE_JOIN_ROUND: join = "round"; break;
	default: join = "miter";
	}
	info.GetReturnValue().Set(nx_str(iso, join));
}
void nx_canvas_context_2d_set_line_join(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	String::Utf8Value type(iso, info[0]);
	if (!*type)
		return;
	cairo_line_join_t lj;
	if (!strcmp("round", *type)) lj = CAIRO_LINE_JOIN_ROUND;
	else if (!strcmp("bevel", *type)) lj = CAIRO_LINE_JOIN_BEVEL;
	else lj = CAIRO_LINE_JOIN_MITER;
	cairo_set_line_join(cr, lj);
}

void nx_canvas_context_2d_get_line_dash_offset(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double offset;
	cairo_get_dash(cr, NULL, &offset);
	info.GetReturnValue().Set(Number::New(iso, offset));
}
void nx_canvas_context_2d_set_line_dash_offset(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double offset;
	if (!info[0]->NumberValue(iso->GetCurrentContext()).To(&offset))
		return;
	int num_dashes = cairo_get_dash_count(cr);
	double *dashes = (double *)alloca(sizeof(double) * (num_dashes ? num_dashes : 1));
	cairo_get_dash(cr, dashes, NULL);
	cairo_set_dash(cr, dashes, num_dashes, offset);
}

void nx_canvas_context_2d_get_line_cap(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	const char *cap;
	switch (cairo_get_line_cap(cr)) {
	case CAIRO_LINE_CAP_ROUND: cap = "round"; break;
	case CAIRO_LINE_CAP_SQUARE: cap = "square"; break;
	default: cap = "butt";
	}
	info.GetReturnValue().Set(nx_str(iso, cap));
}
void nx_canvas_context_2d_set_line_cap(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	String::Utf8Value type(iso, info[0]);
	if (!*type)
		return;
	cairo_line_cap_t lc;
	if (!strcmp("round", *type)) lc = CAIRO_LINE_CAP_ROUND;
	else if (!strcmp("square", *type)) lc = CAIRO_LINE_CAP_SQUARE;
	else lc = CAIRO_LINE_CAP_BUTT;
	cairo_set_line_cap(cr, lc);
}

void nx_canvas_context_2d_get_line_dash(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	Local<Context> jsctx = iso->GetCurrentContext();
	int count = cairo_get_dash_count(cr);
	double *dashes = (double *)alloca(sizeof(double) * (count ? count : 1));
	cairo_get_dash(cr, dashes, NULL);
	Local<Array> array = Array::New(iso, count);
	for (int i = 0; i < count; i++)
		array->Set(jsctx, i, Number::New(iso, dashes[i])).Check();
	info.GetReturnValue().Set(array);
}
void nx_canvas_context_2d_set_line_dash(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	Local<Context> jsctx = iso->GetCurrentContext();
	Local<Object> arr = info[0].As<Object>();
	uint32_t length;
	if (!arr->Get(jsctx, nx_str(iso, "length"))
	         .ToLocalChecked()
	         ->Uint32Value(jsctx)
	         .To(&length))
		return;
	uint32_t num_dashes = length & 1 ? length * 2 : length;
	uint32_t zero_dashes = 0;
	double *dashes =
	    (double *)alloca(sizeof(double) * (num_dashes ? num_dashes : 1));
	for (uint32_t i = 0; i < num_dashes; i++) {
		Local<Value> v = arr->Get(jsctx, i % length).ToLocalChecked();
		if (!v->NumberValue(jsctx).To(&dashes[i]))
			return;
		if (dashes[i] == 0)
			zero_dashes++;
	}
	double offset;
	cairo_get_dash(cr, NULL, &offset);
	if (zero_dashes == num_dashes)
		cairo_set_dash(cr, NULL, 0, offset);
	else
		cairo_set_dash(cr, dashes, num_dashes, offset);
}

void nx_canvas_context_2d_get_global_alpha(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	info.GetReturnValue().Set(Number::New(iso, context->state->global_alpha));
}
void nx_canvas_context_2d_set_global_alpha(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double value;
	if (!info[0]->NumberValue(iso->GetCurrentContext()).To(&value))
		return;
	if (value >= 0 && value <= 1)
		context->state->global_alpha = value;
}

#define SIMPLE_DOUBLE_GETSET(NAME, FIELD, GUARD)                               \
	void nx_canvas_context_2d_get_##NAME(                                      \
	    const FunctionCallbackInfo<Value> &info) {                             \
		ENTER_THIS;                                                            \
		info.GetReturnValue().Set(Number::New(iso, context->state->FIELD));    \
	}                                                                          \
	void nx_canvas_context_2d_set_##NAME(                                      \
	    const FunctionCallbackInfo<Value> &info) {                             \
		ENTER_THIS;                                                            \
		double value;                                                          \
		if (!info[0]->NumberValue(iso->GetCurrentContext()).To(&value))        \
			return;                                                            \
		if (GUARD)                                                             \
			context->state->FIELD = value;                                     \
	}

SIMPLE_DOUBLE_GETSET(shadow_blur, shadow_blur, value >= 0)
SIMPLE_DOUBLE_GETSET(shadow_offset_x, shadow_offset_x, true)
SIMPLE_DOUBLE_GETSET(shadow_offset_y, shadow_offset_y, true)

void nx_canvas_context_2d_get_shadow_color(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_ARGV0;
	Local<Context> jsctx = iso->GetCurrentContext();
	Local<Array> arr = Array::New(iso, 4);
	arr->Set(jsctx, 0, Integer::New(iso, context->state->shadow_color.r * 255)).Check();
	arr->Set(jsctx, 1, Integer::New(iso, context->state->shadow_color.g * 255)).Check();
	arr->Set(jsctx, 2, Integer::New(iso, context->state->shadow_color.b * 255)).Check();
	arr->Set(jsctx, 3, Number::New(iso, context->state->shadow_color.a)).Check();
	info.GetReturnValue().Set(arr);
}
void nx_canvas_context_2d_set_shadow_color(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_ARGV0;
	double a[4];
	if (!get_doubles(info, a, 4, 1))
		return;
	context->state->shadow_color.r = a[0] / 255.;
	context->state->shadow_color.g = a[1] / 255.;
	context->state->shadow_color.b = a[2] / 255.;
	context->state->shadow_color.a = a[3];
}

void nx_canvas_context_2d_get_image_smoothing_enabled(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	info.GetReturnValue().Set(
	    Boolean::New(iso, context->state->image_smoothing_enabled));
}
void nx_canvas_context_2d_set_image_smoothing_enabled(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	context->state->image_smoothing_enabled = info[0]->BooleanValue(iso);
}

void nx_canvas_context_2d_get_image_smoothing_quality(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	const char *q;
	switch (context->state->image_smoothing_quality) {
	case CAIRO_FILTER_BEST: q = "high"; break;
	case CAIRO_FILTER_GOOD: q = "medium"; break;
	default: q = "low";
	}
	info.GetReturnValue().Set(nx_str(iso, q));
}
void nx_canvas_context_2d_set_image_smoothing_quality(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	String::Utf8Value str(iso, info[0]);
	if (!*str)
		return;
	if (!strcmp(*str, "high"))
		context->state->image_smoothing_quality = CAIRO_FILTER_BEST;
	else if (!strcmp(*str, "medium"))
		context->state->image_smoothing_quality = CAIRO_FILTER_GOOD;
	else if (!strcmp(*str, "low"))
		context->state->image_smoothing_quality = CAIRO_FILTER_FAST;
}

void nx_canvas_context_2d_get_miter_limit(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	info.GetReturnValue().Set(Number::New(iso, cairo_get_miter_limit(cr)));
}
void nx_canvas_context_2d_set_miter_limit(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double limit;
	if (!info[0]->NumberValue(iso->GetCurrentContext()).To(&limit))
		return;
	if (limit > 0)
		cairo_set_miter_limit(cr, limit);
}

void nx_canvas_context_2d_get_text_align(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	const char *align;
	switch (context->state->text_align) {
	default:
	case TEXT_ALIGN_START: align = "start"; break;
	case TEXT_ALIGN_LEFT: align = "left"; break;
	case TEXT_ALIGN_CENTER: align = "center"; break;
	case TEXT_ALIGN_RIGHT: align = "right"; break;
	case TEXT_ALIGN_END: align = "end"; break;
	}
	info.GetReturnValue().Set(nx_str(iso, align));
}
void nx_canvas_context_2d_set_text_align(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	String::Utf8Value str(iso, info[0]);
	if (!*str)
		return;
	if (!strcmp(*str, "start")) context->state->text_align = TEXT_ALIGN_START;
	else if (!strcmp(*str, "left")) context->state->text_align = TEXT_ALIGN_LEFT;
	else if (!strcmp(*str, "center")) context->state->text_align = TEXT_ALIGN_CENTER;
	else if (!strcmp(*str, "right")) context->state->text_align = TEXT_ALIGN_RIGHT;
	else if (!strcmp(*str, "end")) context->state->text_align = TEXT_ALIGN_END;
}

void nx_canvas_context_2d_get_text_baseline(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	const char *v;
	switch (context->state->text_baseline) {
	case TEXT_BASELINE_TOP: v = "top"; break;
	case TEXT_BASELINE_BOTTOM: v = "bottom"; break;
	case TEXT_BASELINE_MIDDLE: v = "middle"; break;
	case TEXT_BASELINE_IDEOGRAPHIC: v = "ideographic"; break;
	case TEXT_BASELINE_HANGING: v = "hanging"; break;
	default: v = "alphabetic"; break;
	}
	info.GetReturnValue().Set(nx_str(iso, v));
}
void nx_canvas_context_2d_set_text_baseline(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	String::Utf8Value str(iso, info[0]);
	if (!*str)
		return;
	if (!strcmp(*str, "alphabetic")) context->state->text_baseline = TEXT_BASELINE_ALPHABETIC;
	else if (!strcmp(*str, "top")) context->state->text_baseline = TEXT_BASELINE_TOP;
	else if (!strcmp(*str, "middle")) context->state->text_baseline = TEXT_BASELINE_MIDDLE;
	else if (!strcmp(*str, "bottom")) context->state->text_baseline = TEXT_BASELINE_BOTTOM;
	else if (!strcmp(*str, "ideographic")) context->state->text_baseline = TEXT_BASELINE_IDEOGRAPHIC;
	else if (!strcmp(*str, "hanging")) context->state->text_baseline = TEXT_BASELINE_HANGING;
}

void nx_canvas_context_2d_rotate(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double n;
	if (!info[0]->NumberValue(iso->GetCurrentContext()).To(&n))
		return;
	cairo_rotate(cr, n);
}
void nx_canvas_context_2d_scale(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double a[2];
	if (!get_doubles(info, a, 2, 0))
		return;
	cairo_scale(cr, a[0], a[1]);
}
void nx_canvas_context_2d_translate(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double a[2];
	if (!get_doubles(info, a, 2, 0))
		return;
	cairo_translate(cr, a[0], a[1]);
}
void nx_canvas_context_2d_transform(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double a[6];
	if (!get_doubles(info, a, 6, 0))
		return;
	cairo_matrix_t matrix;
	cairo_matrix_init(&matrix, a[0], a[1], a[2], a[3], a[4], a[5]);
	cairo_transform(cr, &matrix);
}
void nx_canvas_context_2d_reset_transform(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	cairo_identity_matrix(cr);
}
void nx_canvas_context_2d_set_transform(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	if (info.Length() == 1 && info[0]->IsObject()) {
		nx_dommatrix_t *dm = nx_get_dommatrix(iso, info[0]);
		if (dm) {
			cairo_set_matrix(cr, &dm->cr_matrix);
		} else {
			nx_dommatrix_t m;
			if (nx_dommatrix_init(iso, info[0], &m))
				return;
			cairo_set_matrix(cr, &m.cr_matrix);
		}
	} else if (info.Length() == 6) {
		double a[6];
		if (!get_doubles(info, a, 6, 0))
			return;
		cairo_matrix_t m;
		cairo_matrix_init(&m, a[0], a[1], a[2], a[3], a[4], a[5]);
		cairo_set_matrix(cr, &m);
	} else {
		cairo_identity_matrix(cr);
	}
}

void nx_canvas_context_2d_get_global_composite_operation(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	const char *op;
	switch (cairo_get_operator(cr)) {
	case CAIRO_OPERATOR_CLEAR: op = "clear"; break;
	case CAIRO_OPERATOR_SOURCE: op = "copy"; break;
	case CAIRO_OPERATOR_DEST: op = "destination"; break;
	case CAIRO_OPERATOR_OVER: op = "source-over"; break;
	case CAIRO_OPERATOR_DEST_OVER: op = "destination-over"; break;
	case CAIRO_OPERATOR_IN: op = "source-in"; break;
	case CAIRO_OPERATOR_DEST_IN: op = "destination-in"; break;
	case CAIRO_OPERATOR_OUT: op = "source-out"; break;
	case CAIRO_OPERATOR_DEST_OUT: op = "destination-out"; break;
	case CAIRO_OPERATOR_ATOP: op = "source-atop"; break;
	case CAIRO_OPERATOR_DEST_ATOP: op = "destination-atop"; break;
	case CAIRO_OPERATOR_XOR: op = "xor"; break;
	case CAIRO_OPERATOR_ADD: op = "lighter"; break;
	case CAIRO_OPERATOR_MULTIPLY: op = "multiply"; break;
	case CAIRO_OPERATOR_SCREEN: op = "screen"; break;
	case CAIRO_OPERATOR_OVERLAY: op = "overlay"; break;
	case CAIRO_OPERATOR_DARKEN: op = "darken"; break;
	case CAIRO_OPERATOR_LIGHTEN: op = "lighten"; break;
	case CAIRO_OPERATOR_COLOR_DODGE: op = "color-dodge"; break;
	case CAIRO_OPERATOR_COLOR_BURN: op = "color-burn"; break;
	case CAIRO_OPERATOR_HARD_LIGHT: op = "hard-light"; break;
	case CAIRO_OPERATOR_SOFT_LIGHT: op = "soft-light"; break;
	case CAIRO_OPERATOR_DIFFERENCE: op = "difference"; break;
	case CAIRO_OPERATOR_EXCLUSION: op = "exclusion"; break;
	case CAIRO_OPERATOR_HSL_HUE: op = "hue"; break;
	case CAIRO_OPERATOR_HSL_SATURATION: op = "saturation"; break;
	case CAIRO_OPERATOR_HSL_COLOR: op = "color"; break;
	case CAIRO_OPERATOR_HSL_LUMINOSITY: op = "luminosity"; break;
	case CAIRO_OPERATOR_SATURATE: op = "saturate"; break;
	default: op = "source-over";
	}
	info.GetReturnValue().Set(nx_str(iso, op));
}
void nx_canvas_context_2d_set_global_composite_operation(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	int op = -1;
	String::Utf8Value s(iso, info[0]);
	if (!*s)
		return;
	const char *str = *s;
	if (!strcmp(str, "clear")) op = CAIRO_OPERATOR_CLEAR;
	else if (!strcmp(str, "copy")) op = CAIRO_OPERATOR_SOURCE;
	else if (!strcmp(str, "destination")) op = CAIRO_OPERATOR_DEST;
	else if (!strcmp(str, "source-over")) op = CAIRO_OPERATOR_OVER;
	else if (!strcmp(str, "destination-over")) op = CAIRO_OPERATOR_DEST_OVER;
	else if (!strcmp(str, "source-in")) op = CAIRO_OPERATOR_IN;
	else if (!strcmp(str, "destination-in")) op = CAIRO_OPERATOR_DEST_IN;
	else if (!strcmp(str, "source-out")) op = CAIRO_OPERATOR_OUT;
	else if (!strcmp(str, "destination-out")) op = CAIRO_OPERATOR_DEST_OUT;
	else if (!strcmp(str, "source-atop")) op = CAIRO_OPERATOR_ATOP;
	else if (!strcmp(str, "destination-atop")) op = CAIRO_OPERATOR_DEST_ATOP;
	else if (!strcmp(str, "xor")) op = CAIRO_OPERATOR_XOR;
	else if (!strcmp(str, "lighter")) op = CAIRO_OPERATOR_ADD;
	else if (!strcmp(str, "normal")) op = CAIRO_OPERATOR_OVER;
	else if (!strcmp(str, "multiply")) op = CAIRO_OPERATOR_MULTIPLY;
	else if (!strcmp(str, "screen")) op = CAIRO_OPERATOR_SCREEN;
	else if (!strcmp(str, "overlay")) op = CAIRO_OPERATOR_OVERLAY;
	else if (!strcmp(str, "darken")) op = CAIRO_OPERATOR_DARKEN;
	else if (!strcmp(str, "lighten")) op = CAIRO_OPERATOR_LIGHTEN;
	else if (!strcmp(str, "color-dodge")) op = CAIRO_OPERATOR_COLOR_DODGE;
	else if (!strcmp(str, "color-burn")) op = CAIRO_OPERATOR_COLOR_BURN;
	else if (!strcmp(str, "hard-light")) op = CAIRO_OPERATOR_HARD_LIGHT;
	else if (!strcmp(str, "soft-light")) op = CAIRO_OPERATOR_SOFT_LIGHT;
	else if (!strcmp(str, "difference")) op = CAIRO_OPERATOR_DIFFERENCE;
	else if (!strcmp(str, "exclusion")) op = CAIRO_OPERATOR_EXCLUSION;
	else if (!strcmp(str, "hue")) op = CAIRO_OPERATOR_HSL_HUE;
	else if (!strcmp(str, "saturation")) op = CAIRO_OPERATOR_HSL_SATURATION;
	else if (!strcmp(str, "color")) op = CAIRO_OPERATOR_HSL_COLOR;
	else if (!strcmp(str, "luminosity")) op = CAIRO_OPERATOR_HSL_LUMINOSITY;
	else if (!strcmp(str, "saturate")) op = CAIRO_OPERATOR_SATURATE;
	if (op != -1)
		cairo_set_operator(cr, (cairo_operator_t)op);
}

void nx_canvas_context_2d_is_point_in_path(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	// Simplified: path-arg + matrix inversion handling preserved.
	int base = 0;
	Local<Value> path;
	bool have_path = false;
	if (info.Length() > 0 && info[0]->IsObject()) {
		path = info[0];
		have_path = true;
		base = 1;
	}
	bool is_in = false;
	if (info.Length() - base >= 2) {
		double args[2];
		if (!get_doubles(info, args, 2, base))
			return;
		if (info.Length() - base == 3 && info[base + 2]->IsString())
			set_fill_rule_v(iso, info[base + 2], cr);
		bool needs_restore = false;
		if (have_path) {
			needs_restore = true;
			save_path(context);
			apply_path(iso, info.This(), path);
		}
		nx_dommatrix_t matrix = {};
		matrix.is_2d = true;
		matrix.values.m11 = matrix.values.m22 = matrix.values.m33 =
		    matrix.values.m44 = 1.;
		cairo_get_matrix(cr, &matrix.cr_matrix);
		if (!nx_dommatrix_is_identity_(&matrix)) {
			nx_dommatrix_invert_self_(&matrix);
			double z = 0, w = 1;
			nx_dommatrix_transform_point_(&matrix, &args[0], &args[1], &z, &w);
		}
		is_in = cairo_in_fill(cr, args[0], args[1]);
		if (needs_restore)
			restore_path(context);
	}
	info.GetReturnValue().Set(Boolean::New(iso, is_in));
}

void nx_canvas_context_2d_is_point_in_stroke(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	int base = 0;
	Local<Value> path;
	bool have_path = false;
	if (info.Length() > 0 && info[0]->IsObject()) {
		path = info[0];
		have_path = true;
		base = 1;
	}
	bool is_in = false;
	if (info.Length() - base >= 2) {
		double args[2];
		if (!get_doubles(info, args, 2, base))
			return;
		bool needs_restore = false;
		if (have_path) {
			needs_restore = true;
			save_path(context);
			apply_path(iso, info.This(), path);
		}
		nx_dommatrix_t matrix = {};
		matrix.is_2d = true;
		matrix.values.m11 = matrix.values.m22 = matrix.values.m33 =
		    matrix.values.m44 = 1.;
		cairo_get_matrix(cr, &matrix.cr_matrix);
		if (!nx_dommatrix_is_identity_(&matrix)) {
			nx_dommatrix_invert_self_(&matrix);
			double z = 0, w = 1;
			nx_dommatrix_transform_point_(&matrix, &args[0], &args[1], &z, &w);
		}
		is_in = cairo_in_stroke(cr, args[0], args[1]);
		if (needs_restore)
			restore_path(context);
	}
	info.GetReturnValue().Set(Boolean::New(iso, is_in));
}

// ===========================================================================
// ImageData put/get, drawImage, gradients, lifecycle, encode, registration.
// ===========================================================================

void nx_canvas_context_2d_put_image_data(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	Local<Context> jsctx = iso->GetCurrentContext();
	int sx = 0, sy = 0, sw = 0, sh = 0, dx, dy, image_data_width,
	    image_data_height, rows, cols;

	Local<Object> id = info[0].As<Object>();
	Local<Value> data_v = id->Get(jsctx, nx_str(iso, "data")).ToLocalChecked();
	Local<Value> w_v = id->Get(jsctx, nx_str(iso, "width")).ToLocalChecked();
	Local<Value> h_v = id->Get(jsctx, nx_str(iso, "height")).ToLocalChecked();
	if (!data_v->IsArrayBufferView()) {
		nx_throw(iso, "ImageData.data must be a typed array");
		return;
	}
	Local<ArrayBufferView> view = data_v.As<ArrayBufferView>();
	size_t src_offset = view->ByteOffset();
	uint8_t *src =
	    (uint8_t *)view->Buffer()->Data() + src_offset;

	if (!w_v->Int32Value(jsctx).To(&image_data_width) ||
	    !h_v->Int32Value(jsctx).To(&image_data_height) ||
	    !info[1]->Int32Value(jsctx).To(&dx) ||
	    !info[2]->Int32Value(jsctx).To(&dy))
		return;

	uint8_t *dst = context->canvas->data;
	int dstStride = context->canvas->width * 4;
	int srcStride = image_data_width * 4;

	int argc = info.Length();
	if (argc == 3) {
		sw = image_data_width;
		sh = image_data_height;
	} else if (argc == 7) {
		if (!info[3]->Int32Value(jsctx).To(&sx) ||
		    !info[4]->Int32Value(jsctx).To(&sy) ||
		    !info[5]->Int32Value(jsctx).To(&sw) ||
		    !info[6]->Int32Value(jsctx).To(&sh))
			return;
		if (sw < 0) { sx += sw; sw = -sw; }
		if (sh < 0) { sy += sh; sh = -sh; }
		if (sx < 0) { sw += sx; sx = 0; }
		if (sy < 0) { sh += sy; sy = 0; }
		if (sx + sw > image_data_width) sw = image_data_width - sx;
		if (sy + sh > image_data_height) sh = image_data_height - sy;
		dx += sx;
		dy += sy;
	} else {
		nx_throw(iso, "Invalid argument count");
		return;
	}
	if (dx < 0) { sw += dx; sx -= dx; dx = 0; }
	if (dy < 0) { sh += dy; sy -= dy; dy = 0; }
	cols = imin(sw, (int)context->canvas->width - dx);
	rows = imin(sh, (int)context->canvas->height - dy);
	if (cols <= 0 || rows <= 0)
		return;
	src += sy * srcStride + sx * 4;
	dst += dstStride * dy + 4 * dx;
	for (int y = 0; y < rows; ++y) {
		uint8_t *dstRow = dst;
		uint8_t *srcRow = src;
		for (int x = 0; x < cols; ++x) {
			uint8_t r = *srcRow++, g = *srcRow++, b = *srcRow++, a = *srcRow++;
			if (a == 0) {
				*dstRow++ = 0; *dstRow++ = 0; *dstRow++ = 0; *dstRow++ = 0;
			} else if (a == 255) {
				*dstRow++ = b; *dstRow++ = g; *dstRow++ = r; *dstRow++ = a;
			} else {
				float alpha = (float)a / 255;
				*dstRow++ = b * alpha; *dstRow++ = g * alpha;
				*dstRow++ = r * alpha; *dstRow++ = a;
			}
		}
		dst += dstStride;
		src += srcStride;
	}
	cairo_surface_mark_dirty_rectangle(context->canvas->surface, dx, dy, cols,
	                                   rows);
}

void nx_canvas_context_2d_get_image_data(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_ARGV0;
	Local<Context> jsctx = iso->GetCurrentContext();
	uint32_t width = context->canvas->width;
	uint32_t height = context->canvas->height;
	int sx, sy, sw, sh;
	if (!info[1]->Int32Value(jsctx).To(&sx) ||
	    !info[2]->Int32Value(jsctx).To(&sy) ||
	    !info[3]->Int32Value(jsctx).To(&sw) ||
	    !info[4]->Int32Value(jsctx).To(&sh))
		return;
	if (sw < 0) { sx += sw; sw = -sw; }
	if (sh < 0) { sy += sh; sh = -sh; }
	if (sx + sw > (int)width) sw = width - sx;
	if (sy + sh > (int)height) sh = height - sy;
	if (sw <= 0) sw = 1;
	if (sh <= 0) sh = 1;
	if (sx < 0) { sw += sx; sx = 0; }
	if (sy < 0) { sh += sy; sy = 0; }
	int srcStride = width * 4;
	int bpp = 4;
	size_t size = (size_t)sw * sh * bpp;
	int dstStride = sw * bpp;
	uint8_t *src = context->canvas->data;
	Local<ArrayBuffer> ab = ArrayBuffer::New(iso, size);
	uint8_t *dst0 = (uint8_t *)ab->Data();
	uint8_t *dst = dst0;
	for (int y = 0; y < sh; ++y) {
		uint32_t *row = (uint32_t *)(src + srcStride * (y + sy));
		for (int x = 0; x < sw; ++x) {
			int bx = x * 4;
			uint32_t *pixel = row + x + sx;
			uint8_t a = *pixel >> 24, r = *pixel >> 16, g = *pixel >> 8,
			        b = *pixel;
			dst[bx + 3] = a;
			if (a == 0 || a == 255) {
				dst[bx + 0] = r; dst[bx + 1] = g; dst[bx + 2] = b;
			} else {
				float alphaR = (float)255 / a;
				dst[bx + 0] = (int)((float)r * alphaR);
				dst[bx + 1] = (int)((float)g * alphaR);
				dst[bx + 2] = (int)((float)b * alphaR);
			}
		}
		dst += dstStride;
	}
	info.GetReturnValue().Set(ab);
}

void decompose_matrix(cairo_matrix_t matrix, double *destination) {
	double denom = pow(matrix.xx, 2) + pow(matrix.yx, 2);
	destination[0] = atan2(matrix.yx, matrix.xx);
	destination[1] = sqrt(denom);
	destination[2] =
	    (matrix.xx * matrix.yy - matrix.xy * matrix.yx) / destination[1];
	destination[3] =
	    atan2(matrix.xx * matrix.xy + matrix.yx * matrix.yy, denom);
	destination[4] = matrix.x0;
	destination[5] = matrix.y0;
}

void nx_canvas_context_2d_draw_image(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	int argc = info.Length();
	if (argc != 3 && argc != 5 && argc != 9) {
		nx_throw(iso, "Invalid arguments");
		return;
	}
	double args[8];
	if (!get_doubles(info, args, argc - 1, 1))
		return;
	Ctx2D _c = enter_ctx(iso, info.This());
	if (!_c.ok) {
		if (!_c.noop && !_c.context)
			nx_throw(iso, "invalid canvas context");
		return;
	}
	nx_canvas_context_2d_t *context = _c.context;
	cairo_t *cr = _c.cr;
	double sx = 0, sy = 0, sw = 0, sh = 0, dx = 0, dy = 0, dw = 0, dh = 0,
	       source_w = 0, source_h = 0;
	cairo_surface_t *surface;
	nx_image_t *img = nx_get_image(iso, info[0]);
	if (img) {
		surface = img->surface;
		source_w = sw = img->width;
		source_h = sh = img->height;
	} else {
		nx_canvas_t *canvas = nx_get_canvas(iso, info[0]);
		if (!canvas) {
			nx_throw(iso, "Image or Canvas expected");
			return;
		}
		surface = canvas->surface;
		source_w = sw = canvas->width;
		source_h = sh = canvas->height;
	}
	switch (argc) {
	case 9:
		sx = args[0]; sy = args[1]; sw = args[2]; sh = args[3];
		dx = args[4]; dy = args[5]; dw = args[6]; dh = args[7];
		break;
	case 5:
		dx = args[0]; dy = args[1]; dw = args[2]; dh = args[3];
		break;
	case 3:
		dx = args[0]; dy = args[1]; dw = sw; dh = sh;
		break;
	}
	if (!(sw && sh && dw && dh))
		return;
	cairo_save(cr);
	cairo_matrix_t matrix;
	double transforms[6];
	cairo_get_matrix(cr, &matrix);
	decompose_matrix(matrix, transforms);
	double current_scale_x = fabs(transforms[1]);
	double current_scale_y = fabs(transforms[2]);
	double extra_dx = 0, extra_dy = 0;
	double fx = dw / sw * current_scale_x;
	double fy = dh / sh * current_scale_y;
	bool needScale = dw != sw || dh != sh;
	bool needCut = sw != source_w || sh != source_h || sx < 0 || sy < 0;
	bool sameCanvas = surface == context->canvas->surface;
	bool needsExtraSurface = sameCanvas || needCut || needScale;
	cairo_surface_t *surfTemp = NULL;
	cairo_t *ctxTemp = NULL;
	if (needsExtraSurface) {
		double real_w = sw, real_h = sh, translate_x = 0, translate_y = 0;
		if (sx < 0) { extra_dx = -sx * fx; real_w = sw + sx; }
		else if (sx + sw > source_w) real_w = sw - (sx + sw - source_w);
		if (sy < 0) { extra_dy = -sy * fy; real_h = sh + sy; }
		else if (sy + sh > source_h) real_h = sh - (sy + sh - source_h);
		if (real_w > source_w) real_w = source_w;
		if (real_h > source_h) real_h = source_h;
		surfTemp = cairo_image_surface_create(
		    CAIRO_FORMAT_ARGB32, round(real_w * fx), round(real_h * fy));
		ctxTemp = cairo_create(surfTemp);
		cairo_scale(ctxTemp, fx, fy);
		if (sx > 0) translate_x = sx;
		if (sy > 0) translate_y = sy;
		cairo_set_source_surface(ctxTemp, surface, -translate_x, -translate_y);
		cairo_pattern_set_filter(cairo_get_source(ctxTemp),
		                         context->state->image_smoothing_enabled
		                             ? context->state->image_smoothing_quality
		                             : CAIRO_FILTER_NEAREST);
		cairo_pattern_set_extend(cairo_get_source(ctxTemp),
		                         CAIRO_EXTEND_REFLECT);
		cairo_paint_with_alpha(ctxTemp, 1);
		surface = surfTemp;
	}
	double scaled_dx = dx, scaled_dy = dy;
	if (needsExtraSurface && (current_scale_x != 1 || current_scale_y != 1)) {
		cairo_scale(cr, 1 / current_scale_x, 1 / current_scale_y);
		scaled_dx *= current_scale_x;
		scaled_dy *= current_scale_y;
	}
	cairo_set_source_surface(cr, surface, scaled_dx + extra_dx,
	                         scaled_dy + extra_dy);
	cairo_pattern_set_filter(cairo_get_source(cr),
	                         context->state->image_smoothing_enabled
	                             ? context->state->image_smoothing_quality
	                             : CAIRO_FILTER_NEAREST);
	cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_NONE);
	cairo_paint_with_alpha(cr, context->state->global_alpha);
	cairo_restore(cr);
	if (needsExtraSurface) {
		cairo_destroy(ctxTemp);
		cairo_surface_destroy(surfTemp);
	}
}

// ---- lifecycle ----
void free_canvas(nx_canvas_t *c) {
	if (c->surface)
		cairo_surface_destroy(c->surface);
	if (c->data)
		free(c->data);
	free(c);
}

void free_context_state(nx_canvas_context_2d_state_t *state) {
	if (state->next)
		free_context_state(state->next);
	if (state->font_string)
		free((void *)state->font_string);
	if (state->fill_gradient)
		cairo_pattern_destroy(state->fill_gradient);
	if (state->stroke_gradient)
		cairo_pattern_destroy(state->stroke_gradient);
	free(state);
}

void free_context_2d(nx_canvas_context_2d_t *context) {
	if (context->ctx)
		cairo_destroy(context->ctx);
	free_context_state(context->state);
	free(context);
}

void free_gradient(cairo_pattern_t *p) {
	if (p)
		cairo_pattern_destroy(p);
}

void nx_canvas_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> jsctx = iso->GetCurrentContext();
	uint32_t width, height;
	if (!info[0]->Uint32Value(jsctx).To(&width) ||
	    !info[1]->Uint32Value(jsctx).To(&height))
		return;
	if (width == 0 || height == 0 || width > SIZE_MAX / 4 ||
	    (size_t)height > SIZE_MAX / ((size_t)width * 4)) {
		iso->ThrowException(
		    Exception::RangeError(nx_str(iso, "Canvas dimensions too large")));
		return;
	}
	size_t buf_size = (size_t)width * height * 4;
	uint8_t *buffer = (uint8_t *)calloc(1, buf_size);
	if (!buffer) {
		nx_throw(iso, "out of memory");
		return;
	}
	nx_canvas_t *canvas = (nx_canvas_t *)calloc(1, sizeof(nx_canvas_t));
	canvas->width = width;
	canvas->height = height;
	canvas->data = buffer;
	canvas->surface = cairo_image_surface_create_for_data(
	    buffer, CAIRO_FORMAT_ARGB32, width, height, width * 4);
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_canvas_t>(iso, obj, canvas, free_canvas);
	info.GetReturnValue().Set(obj);
}

void nx_canvas_get_width(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_canvas_t *canvas = nx_get_canvas(iso, info.This());
	if (canvas)
		info.GetReturnValue().Set(Integer::NewFromUnsigned(iso, canvas->width));
}
void nx_canvas_set_width(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_canvas_t *canvas = nx_get_canvas(iso, info.This());
	if (!canvas)
		return;
	uint32_t w;
	if (!info[0]->Uint32Value(iso->GetCurrentContext()).To(&w))
		return;
	canvas->width = w;
	canvas->surface_dirty = true;
}
void nx_canvas_get_height(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_canvas_t *canvas = nx_get_canvas(iso, info.This());
	if (canvas)
		info.GetReturnValue().Set(Integer::NewFromUnsigned(iso, canvas->height));
}
void nx_canvas_set_height(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_canvas_t *canvas = nx_get_canvas(iso, info.This());
	if (!canvas)
		return;
	uint32_t h;
	if (!info[0]->Uint32Value(iso->GetCurrentContext()).To(&h))
		return;
	canvas->height = h;
	canvas->surface_dirty = true;
}

void nx_canvas_proto_to_data_url(const FunctionCallbackInfo<Value> &info);

void nx_canvas_init_class(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> jsctx = iso->GetCurrentContext();
	Local<Object> proto = info[0]
	                          .As<Object>()
	                          ->Get(jsctx, nx_str(iso, "prototype"))
	                          .ToLocalChecked()
	                          .As<Object>();
	NX_DEF_GETSET(proto, "width", nx_canvas_get_width, nx_canvas_set_width);
	NX_DEF_GETSET(proto, "height", nx_canvas_get_height, nx_canvas_set_height);
	NX_DEF_FUNC(proto, "toDataURL", nx_canvas_proto_to_data_url, 0);
}

void nx_canvas_context_2d_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_canvas_t *canvas = nx_get_canvas(iso, info[0]);
	if (!canvas) {
		nx_throw(iso, "Expected a canvas object");
		return;
	}
	nx_canvas_context_2d_t *context =
	    (nx_canvas_context_2d_t *)calloc(1, sizeof(nx_canvas_context_2d_t));
	nx_canvas_context_2d_state_t *state =
	    (nx_canvas_context_2d_state_t *)calloc(
	        1, sizeof(nx_canvas_context_2d_state_t));
	context->canvas = canvas;
	context->state = state;
	context->ctx = cairo_create(canvas->surface);
	state->font_size = 10.;
	state->fill.a = 1.;
	state->stroke.a = 1.;
	state->fill_source_type = SOURCE_RGBA;
	state->stroke_source_type = SOURCE_RGBA;
	state->global_alpha = 1.;
	state->image_smoothing_quality = CAIRO_FILTER_FAST;
	state->image_smoothing_enabled = true;
	state->text_align = TEXT_ALIGN_START;
	state->text_baseline = TEXT_BASELINE_ALPHABETIC;
	cairo_set_line_width(context->ctx, 1.);
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_canvas_context_2d_t>(iso, obj, context, free_context_2d);
	info.GetReturnValue().Set(obj);
}

void nx_canvas_context_2d_init_class(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> jsctx = iso->GetCurrentContext();
	Local<Object> proto = info[0]
	                          .As<Object>()
	                          ->Get(jsctx, nx_str(iso, "prototype"))
	                          .ToLocalChecked()
	                          .As<Object>();
#define G(NAME, GET) NX_DEF_GET(proto, NAME, GET)
#define GS(NAME, GET, SET) NX_DEF_GETSET(proto, NAME, GET, SET)
#define F(NAME, FN, N) NX_DEF_FUNC(proto, NAME, FN, N)
	GS("globalAlpha", nx_canvas_context_2d_get_global_alpha,
	   nx_canvas_context_2d_set_global_alpha);
	GS("globalCompositeOperation",
	   nx_canvas_context_2d_get_global_composite_operation,
	   nx_canvas_context_2d_set_global_composite_operation);
	GS("imageSmoothingEnabled",
	   nx_canvas_context_2d_get_image_smoothing_enabled,
	   nx_canvas_context_2d_set_image_smoothing_enabled);
	GS("imageSmoothingQuality",
	   nx_canvas_context_2d_get_image_smoothing_quality,
	   nx_canvas_context_2d_set_image_smoothing_quality);
	GS("lineCap", nx_canvas_context_2d_get_line_cap,
	   nx_canvas_context_2d_set_line_cap);
	GS("lineDashOffset", nx_canvas_context_2d_get_line_dash_offset,
	   nx_canvas_context_2d_set_line_dash_offset);
	GS("lineJoin", nx_canvas_context_2d_get_line_join,
	   nx_canvas_context_2d_set_line_join);
	GS("lineWidth", nx_canvas_context_2d_get_line_width,
	   nx_canvas_context_2d_set_line_width);
	GS("miterLimit", nx_canvas_context_2d_get_miter_limit,
	   nx_canvas_context_2d_set_miter_limit);
	GS("shadowBlur", nx_canvas_context_2d_get_shadow_blur,
	   nx_canvas_context_2d_set_shadow_blur);
	GS("shadowOffsetX", nx_canvas_context_2d_get_shadow_offset_x,
	   nx_canvas_context_2d_set_shadow_offset_x);
	GS("shadowOffsetY", nx_canvas_context_2d_get_shadow_offset_y,
	   nx_canvas_context_2d_set_shadow_offset_y);
	GS("textAlign", nx_canvas_context_2d_get_text_align,
	   nx_canvas_context_2d_set_text_align);
	GS("textBaseline", nx_canvas_context_2d_get_text_baseline,
	   nx_canvas_context_2d_set_text_baseline);
	F("arc", nx_canvas_context_2d_arc, 5);
	F("arcTo", nx_canvas_context_2d_arc_to, 5);
	F("beginPath", nx_canvas_context_2d_begin_path, 0);
	F("bezierCurveTo", nx_canvas_context_2d_bezier_curve_to, 6);
	F("clearRect", nx_canvas_context_2d_clear_rect, 4);
	F("closePath", nx_canvas_context_2d_close_path, 0);
	F("clip", nx_canvas_context_2d_clip, 0);
	F("drawImage", nx_canvas_context_2d_draw_image, 3);
	F("ellipse", nx_canvas_context_2d_ellipse, 7);
	F("fill", nx_canvas_context_2d_fill, 0);
	F("fillRect", nx_canvas_context_2d_fill_rect, 4);
	F("fillText", nx_canvas_context_2d_fill_text, 3);
	F("getLineDash", nx_canvas_context_2d_get_line_dash, 0);
	F("isPointInPath", nx_canvas_context_2d_is_point_in_path, 2);
	F("isPointInStroke", nx_canvas_context_2d_is_point_in_stroke, 2);
	F("lineTo", nx_canvas_context_2d_line_to, 2);
	F("measureText", nx_canvas_context_2d_measure_text, 1);
	F("moveTo", nx_canvas_context_2d_move_to, 2);
	F("putImageData", nx_canvas_context_2d_put_image_data, 3);
	F("quadraticCurveTo", nx_canvas_context_2d_quadratic_curve_to, 4);
	F("rect", nx_canvas_context_2d_rect, 4);
	F("resetTransform", nx_canvas_context_2d_reset_transform, 0);
	F("restore", nx_canvas_context_2d_restore, 0);
	F("rotate", nx_canvas_context_2d_rotate, 1);
	F("roundRect", nx_canvas_context_2d_round_rect, 4);
	F("save", nx_canvas_context_2d_save, 0);
	F("scale", nx_canvas_context_2d_scale, 2);
	F("setLineDash", nx_canvas_context_2d_set_line_dash, 1);
	F("setTransform", nx_canvas_context_2d_set_transform, 0);
	F("stroke", nx_canvas_context_2d_stroke, 0);
	F("strokeRect", nx_canvas_context_2d_stroke_rect, 4);
	F("strokeText", nx_canvas_context_2d_stroke_text, 3);
	F("transform", nx_canvas_context_2d_transform, 6);
	F("translate", nx_canvas_context_2d_translate, 2);
#undef G
#undef GS
#undef F
}

// ---- gradient ----
void nx_canvas_gradient_new_linear(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	double a[4];
	if (!get_doubles(info, a, 4, 0))
		return;
	cairo_pattern_t *pattern =
	    cairo_pattern_create_linear(a[0], a[1], a[2], a[3]);
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<cairo_pattern_t>(iso, obj, pattern, free_gradient);
	info.GetReturnValue().Set(obj);
}
void nx_canvas_gradient_new_radial(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	double a[6];
	if (!get_doubles(info, a, 6, 0))
		return;
	cairo_pattern_t *pattern =
	    cairo_pattern_create_radial(a[0], a[1], a[2], a[3], a[4], a[5]);
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<cairo_pattern_t>(iso, obj, pattern, free_gradient);
	info.GetReturnValue().Set(obj);
}
void nx_canvas_gradient_add_color_stop(
    const FunctionCallbackInfo<Value> &info) {
	cairo_pattern_t *pattern = nx::Unwrap<cairo_pattern_t>(info[0]);
	if (!pattern)
		return;
	double a[5];
	if (!get_doubles(info, a, 5, 1))
		return;
	cairo_pattern_add_color_stop_rgba(pattern, a[0], a[1] / 255., a[2] / 255.,
	                                  a[3] / 255., a[4]);
}
void nx_canvas_gradient_init_class(const FunctionCallbackInfo<Value> &info) {
	(void)info;
}
void nx_canvas_context_2d_set_fill_style_gradient(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_ARGV0;
	cairo_pattern_t *pattern = nx::Unwrap<cairo_pattern_t>(info[1]);
	if (!pattern)
		return;
	if (context->state->fill_gradient)
		cairo_pattern_destroy(context->state->fill_gradient);
	cairo_pattern_reference(pattern);
	context->state->fill_gradient = pattern;
	context->state->fill_source_type = SOURCE_GRADIENT;
}
void nx_canvas_context_2d_set_stroke_style_gradient(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_ARGV0;
	cairo_pattern_t *pattern = nx::Unwrap<cairo_pattern_t>(info[1]);
	if (!pattern)
		return;
	if (context->state->stroke_gradient)
		cairo_pattern_destroy(context->state->stroke_gradient);
	cairo_pattern_reference(pattern);
	context->state->stroke_gradient = pattern;
	context->state->stroke_source_type = SOURCE_GRADIENT;
}

// ---- encode (PNG/JPEG/WebP) ----
typedef struct {
	uint8_t *data;
	size_t size, capacity;
} png_write_buffer_t;
static cairo_status_t png_write_callback(void *closure,
                                         const unsigned char *data,
                                         unsigned int length) {
	png_write_buffer_t *buf = (png_write_buffer_t *)closure;
	size_t needed = buf->size + length;
	if (needed > buf->capacity) {
		size_t new_cap = buf->capacity * 2;
		if (new_cap < needed)
			new_cap = needed;
		uint8_t *nd = (uint8_t *)realloc(buf->data, new_cap);
		if (!nd)
			return CAIRO_STATUS_NO_MEMORY;
		buf->data = nd;
		buf->capacity = new_cap;
	}
	memcpy(buf->data + buf->size, data, length);
	buf->size += length;
	return CAIRO_STATUS_SUCCESS;
}
static int encode_png_buf(cairo_surface_t *surface, uint8_t **out,
                          size_t *out_size) {
	png_write_buffer_t buf = {NULL, 0, 4096};
	buf.data = (uint8_t *)malloc(buf.capacity);
	if (!buf.data)
		return -1;
	cairo_status_t status =
	    cairo_surface_write_to_png_stream(surface, png_write_callback, &buf);
	if (status != CAIRO_STATUS_SUCCESS) {
		free(buf.data);
		return -1;
	}
	*out = buf.data;
	*out_size = buf.size;
	return 0;
}
static uint8_t *bgra_to_rgb(const uint8_t *bgra, int width, int height,
                            int stride) {
	uint8_t *rgb = (uint8_t *)malloc(width * height * 3);
	if (!rgb)
		return NULL;
	for (int y = 0; y < height; y++) {
		const uint8_t *row = bgra + y * stride;
		for (int x = 0; x < width; x++) {
			uint8_t b = row[x * 4 + 0], g = row[x * 4 + 1], r = row[x * 4 + 2],
			        a = row[x * 4 + 3];
			if (a == 0) {
				rgb[(y * width + x) * 3 + 0] = 255;
				rgb[(y * width + x) * 3 + 1] = 255;
				rgb[(y * width + x) * 3 + 2] = 255;
			} else if (a == 255) {
				rgb[(y * width + x) * 3 + 0] = r;
				rgb[(y * width + x) * 3 + 1] = g;
				rgb[(y * width + x) * 3 + 2] = b;
			} else {
				float inv_a = 1.0f - a / 255.0f;
				rgb[(y * width + x) * 3 + 0] = (uint8_t)(r + 255 * inv_a + 0.5f);
				rgb[(y * width + x) * 3 + 1] = (uint8_t)(g + 255 * inv_a + 0.5f);
				rgb[(y * width + x) * 3 + 2] = (uint8_t)(b + 255 * inv_a + 0.5f);
			}
		}
	}
	return rgb;
}
static int encode_jpeg_buf(cairo_surface_t *surface, int quality, uint8_t **out,
                           size_t *out_size) {
	int width = cairo_image_surface_get_width(surface);
	int height = cairo_image_surface_get_height(surface);
	int stride = cairo_image_surface_get_stride(surface);
	uint8_t *data = cairo_image_surface_get_data(surface);
	uint8_t *rgb = bgra_to_rgb(data, width, height, stride);
	if (!rgb)
		return -1;
	tjhandle handle = tjInitCompress();
	if (!handle) {
		free(rgb);
		return -1;
	}
	unsigned char *jpeg_buf = NULL;
	unsigned long jpeg_size = 0;
	int result = tjCompress2(handle, rgb, width, 0, height, TJPF_RGB, &jpeg_buf,
	                         &jpeg_size, TJSAMP_420, quality, TJFLAG_FASTDCT);
	free(rgb);
	tjDestroy(handle);
	if (result != 0) {
		if (jpeg_buf)
			tjFree(jpeg_buf);
		return -1;
	}
	*out = jpeg_buf;
	*out_size = jpeg_size;
	return 0;
}
static int encode_webp_buf(cairo_surface_t *surface, float quality,
                           uint8_t **out, size_t *out_size) {
	int width = cairo_image_surface_get_width(surface);
	int height = cairo_image_surface_get_height(surface);
	int stride = cairo_image_surface_get_stride(surface);
	uint8_t *data = cairo_image_surface_get_data(surface);
	uint8_t *output = NULL;
	size_t output_size =
	    WebPEncodeBGRA(data, width, height, stride, quality, &output);
	if (output_size == 0 || !output)
		return -1;
	*out = output;
	*out_size = output_size;
	return 0;
}
static int encode_surface(cairo_surface_t *surface, int type, double quality,
                          uint8_t **out, size_t *out_size) {
	switch (type) {
	case 1: {
		uint8_t *jpeg_buf = NULL;
		size_t jpeg_size = 0;
		if (encode_jpeg_buf(surface, (int)(quality * 100), &jpeg_buf,
		                    &jpeg_size) != 0)
			return -1;
		*out = (uint8_t *)malloc(jpeg_size);
		if (!*out) {
			tjFree(jpeg_buf);
			return -1;
		}
		memcpy(*out, jpeg_buf, jpeg_size);
		*out_size = jpeg_size;
		tjFree(jpeg_buf);
		return 0;
	}
	case 2: {
		uint8_t *webp_buf = NULL;
		size_t webp_size = 0;
		if (encode_webp_buf(surface, (float)(quality * 100), &webp_buf,
		                    &webp_size) != 0)
			return -1;
		*out = (uint8_t *)malloc(webp_size);
		if (!*out) {
			WebPFree(webp_buf);
			return -1;
		}
		memcpy(*out, webp_buf, webp_size);
		*out_size = webp_size;
		WebPFree(webp_buf);
		return 0;
	}
	default:
		return encode_png_buf(surface, out, out_size);
	}
}
static int mime_to_type_code(const char *type) {
	if (type) {
		if (!strcmp(type, "image/jpeg"))
			return 1;
		if (!strcmp(type, "image/webp"))
			return 2;
	}
	return 0;
}
static const char *type_code_to_mime(int type) {
	switch (type) {
	case 1: return "image/jpeg";
	case 2: return "image/webp";
	default: return "image/png";
	}
}
static cairo_surface_t *snapshot_surface(nx_canvas_t *canvas) {
	if (!canvas->surface || canvas->width == 0 || canvas->height == 0)
		return cairo_image_surface_create(
		    CAIRO_FORMAT_ARGB32, canvas->width ? canvas->width : 1,
		    canvas->height ? canvas->height : 1);
	cairo_surface_flush(canvas->surface);
	int w = cairo_image_surface_get_width(canvas->surface);
	int h = cairo_image_surface_get_height(canvas->surface);
	int stride = cairo_image_surface_get_stride(canvas->surface);
	uint8_t *src = cairo_image_surface_get_data(canvas->surface);
	cairo_surface_t *copy = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
	uint8_t *dst = cairo_image_surface_get_data(copy);
	int dst_stride = cairo_image_surface_get_stride(copy);
	for (int y = 0; y < h; y++)
		memcpy(dst + y * dst_stride, src + y * stride, w * 4);
	cairo_surface_mark_dirty(copy);
	return copy;
}

typedef struct {
	cairo_surface_t *snapshot;
	int type;
	double quality;
	uint8_t *result;
	size_t result_size;
	int err;
} encode_async_t;

void nx_canvas_encode_do(nx_work_t *req) {
	encode_async_t *data = (encode_async_t *)req->data;
	data->err = encode_surface(data->snapshot, data->type, data->quality,
	                           &data->result, &data->result_size);
}
MaybeLocal<Value> nx_canvas_encode_cb(Isolate *iso, nx_work_t *req) {
	encode_async_t *data = (encode_async_t *)req->data;
	cairo_surface_destroy(data->snapshot);
	if (data->err) {
		nx_throw(iso, "Failed to encode image");
		return MaybeLocal<Value>();
	}
	uint8_t *buf = data->result;
	size_t size = data->result_size;
	data->result = nullptr;
	std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
	    buf, size, [](void *p, size_t, void *) { free(p); }, nullptr);
	return ArrayBuffer::New(iso, std::move(bs)).As<Value>();
}

void nx_canvas_to_buffer(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_canvas_t *canvas = nx_get_canvas(iso, info[0]);
	if (!canvas) {
		nx_throw(iso, "Expected a canvas object");
		return;
	}
	int type_code = 0;
	double quality = 0.92;
	if (info.Length() > 1 && !info[1]->IsUndefined()) {
		String::Utf8Value type_str(iso, info[1]);
		type_code = mime_to_type_code(*type_str);
	}
	if (info.Length() > 2)
		if (!info[2]->NumberValue(iso->GetCurrentContext()).To(&quality)) quality = 0.92;
	if (quality < 0.0) quality = 0.0;
	if (quality > 1.0) quality = 1.0;
	NX_INIT_WORK_T(encode_async_t);
	data->snapshot = snapshot_surface(canvas);
	data->type = type_code;
	data->quality = quality;
	info.GetReturnValue().Set(
	    nx_queue_async(iso, req, nx_canvas_encode_do, nx_canvas_encode_cb));
}

void nx_canvas_proto_to_data_url(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_canvas_t *canvas = nx_get_canvas(iso, info.This());
	if (!canvas) {
		nx_throw(iso, "Expected a canvas object");
		return;
	}
	int type_code = 0;
	double quality = 0.92;
	if (info.Length() > 0 && !info[0]->IsUndefined()) {
		String::Utf8Value type_str(iso, info[0]);
		type_code = mime_to_type_code(*type_str);
	}
	if (info.Length() > 1)
		if (!info[1]->NumberValue(iso->GetCurrentContext()).To(&quality)) quality = 0.92;
	if (quality < 0.0) quality = 0.0;
	if (quality > 1.0) quality = 1.0;
	cairo_surface_t *snapshot = snapshot_surface(canvas);
	uint8_t *buf = NULL;
	size_t buf_size = 0;
	if (encode_surface(snapshot, type_code, quality, &buf, &buf_size) != 0) {
		cairo_surface_destroy(snapshot);
		nx_throw(iso, "Failed to encode data URL");
		return;
	}
	cairo_surface_destroy(snapshot);
	const char *mime = type_code_to_mime(type_code);
	size_t b64_len = 0;
	mbedtls_base64_encode(NULL, 0, &b64_len, buf, buf_size);
	size_t prefix_len = 5 + strlen(mime) + 8;
	char *data_url = (char *)malloc(prefix_len + b64_len + 1);
	if (!data_url) {
		free(buf);
		nx_throw(iso, "Out of memory");
		return;
	}
	int written = sprintf(data_url, "data:%s;base64,", mime);
	size_t actual_len = 0;
	mbedtls_base64_encode((unsigned char *)data_url + written, b64_len + 1,
	                      &actual_len, buf, buf_size);
	data_url[written + actual_len] = '\0';
	free(buf);
	info.GetReturnValue().Set(nx_str(iso, data_url));
	free(data_url);
}

} // namespace

// ---- shared (non-namespace) symbols ----
nx_canvas_t *nx_get_canvas(Isolate *iso, Local<Value> obj) {
	(void)iso;
	return nx::Unwrap<nx_canvas_t>(obj);
}
nx_canvas_context_2d_t *nx_get_canvas_context_2d(Isolate *iso,
                                                 Local<Value> obj) {
	(void)iso;
	return nx::Unwrap<nx_canvas_context_2d_t>(obj);
}
uint8_t *nx_canvas_pixels(nx_canvas_t *c) { return c ? c->data : nullptr; }
uint32_t nx_canvas_width(nx_canvas_t *c) { return c ? c->width : 0; }
uint32_t nx_canvas_height(nx_canvas_t *c) { return c ? c->height : 0; }

void nx_init_canvas(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "canvasNew", nx_canvas_new);
	NX_SET_FUNC(init_obj, "canvasInitClass", nx_canvas_init_class);
	NX_SET_FUNC(init_obj, "canvasContext2dNew", nx_canvas_context_2d_new);
	NX_SET_FUNC(init_obj, "canvasContext2dInitClass",
	            nx_canvas_context_2d_init_class);
	NX_SET_FUNC(init_obj, "canvasContext2dGetImageData",
	            nx_canvas_context_2d_get_image_data);
	NX_SET_FUNC(init_obj, "canvasContext2dGetTransform",
	            nx_canvas_context_2d_get_transform);
	NX_SET_FUNC(init_obj, "canvasContext2dGetFont",
	            nx_canvas_context_2d_get_font);
	NX_SET_FUNC(init_obj, "canvasContext2dSetFont",
	            nx_canvas_context_2d_set_font);
	NX_SET_FUNC(init_obj, "canvasContext2dGetFillStyle",
	            nx_canvas_context_2d_get_fill_style);
	NX_SET_FUNC(init_obj, "canvasContext2dSetFillStyle",
	            nx_canvas_context_2d_set_fill_style);
	NX_SET_FUNC(init_obj, "canvasContext2dGetStrokeStyle",
	            nx_canvas_context_2d_get_stroke_style);
	NX_SET_FUNC(init_obj, "canvasContext2dSetStrokeStyle",
	            nx_canvas_context_2d_set_stroke_style);
	NX_SET_FUNC(init_obj, "canvasContext2dGetShadowColor",
	            nx_canvas_context_2d_get_shadow_color);
	NX_SET_FUNC(init_obj, "canvasContext2dSetShadowColor",
	            nx_canvas_context_2d_set_shadow_color);
	NX_SET_FUNC(init_obj, "canvasContext2dSetFillStyleGradient",
	            nx_canvas_context_2d_set_fill_style_gradient);
	NX_SET_FUNC(init_obj, "canvasContext2dSetStrokeStyleGradient",
	            nx_canvas_context_2d_set_stroke_style_gradient);
	NX_SET_FUNC(init_obj, "canvasGradientNewLinear",
	            nx_canvas_gradient_new_linear);
	NX_SET_FUNC(init_obj, "canvasGradientNewRadial",
	            nx_canvas_gradient_new_radial);
	NX_SET_FUNC(init_obj, "canvasGradientInitClass",
	            nx_canvas_gradient_init_class);
	NX_SET_FUNC(init_obj, "canvasGradientAddColorStop",
	            nx_canvas_gradient_add_color_stop);
	NX_SET_FUNC(init_obj, "canvasToBuffer", nx_canvas_to_buffer);
}
