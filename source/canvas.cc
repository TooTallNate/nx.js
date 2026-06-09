/**
 * Canvas 2D — V8 binding over the Skia (raster) rendering body (Phase 2.1).
 *
 * Ported from the Cairo implementation (originally node-canvas, MIT). Every
 * canvas is backed by a raster SkSurface over `nx_canvas_t::data` (kBGRA_8888 /
 * premultiplied, byte layout identical to the former CAIRO_FORMAT_ARGB32), so
 * the screen present path stays a memcpy and get/putImageData stay cheap CPU
 * ops. GPU backing + readback demotion is a Phase 2.2 follow-up; the drawing
 * body here is backing-agnostic and will not change for it.
 */
#include "canvas.h"
#include "async.h"
#include "canvas_path.h"
#include "dommatrix.h"
#include "error.h"
#include "font.h"
#include "path2d.h"
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

#include "include/core/SkColor.h"
#include "include/core/SkData.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageFilter.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkRRect.h"
#include "include/core/SkRect.h"
#include "include/core/SkSurface.h"
#include "include/core/SkPathUtils.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkStream.h"
#include "include/core/SkTextBlob.h"
#include "include/core/SkTileMode.h"
#include "include/effects/SkDashPathEffect.h"
#include "include/effects/SkGradient.h"
#include "include/effects/SkImageFilters.h"
#include "include/encode/SkPngEncoder.h"

#include <harfbuzz/hb.h>
#include <utility>
#include <vector>

using namespace v8;

namespace {

typedef struct Point {
	float x, y;
} Point;

static inline int imin(int a, int b) { return a < b ? a : b; }
[[maybe_unused]] static inline void point_swap(Point *a, Point *b) {
	Point t = *a;
	*a = *b;
	*b = t;
}

static void set_font_size(nx_canvas_context_2d_t *context, double font_size);

// ---------------------------------------------------------------------------
// Color / pixel helpers
// ---------------------------------------------------------------------------

// nx_rgba_t stores r/g/b AND a all in the 0..1 range (the JS boundary divides
// the 0..255 byte channels by 255 on the way in). Build a non-premultiplied
// SkColor; Skia premultiplies into the kPremul surface at draw time.
static inline U8CPU clamp_u8(double v01) {
	double v = v01 * 255.0 + 0.5;
	if (v < 0) v = 0;
	if (v > 255) v = 255;
	return (U8CPU)v;
}
static inline SkColor rgba_to_color(const nx_rgba_t &c, double extra_alpha) {
	double a = c.a * extra_alpha;
	if (a < 0) a = 0;
	if (a > 1) a = 1;
	return SkColorSetARGB((U8CPU)(a * 255.0 + 0.5), clamp_u8(c.r), clamp_u8(c.g),
	                      clamp_u8(c.b));
}

// ---------------------------------------------------------------------------
// Raster surface (re)creation after a resize. Replaces the Cairo body.
// ---------------------------------------------------------------------------
static void nx_canvas_ensure_surface(Isolate *iso,
                                     nx_canvas_context_2d_t *context);

// Build an SkSurface that draws directly into `canvas->data`.
static sk_sp<SkSurface> make_raster_surface(uint8_t *data, uint32_t w,
                                            uint32_t h) {
	SkImageInfo info = SkImageInfo::Make(w, h, kBGRA_8888_SkColorType,
	                                     kPremul_SkAlphaType);
	return SkSurfaces::WrapPixels(info, data, (size_t)w * 4, nullptr);
}

// Return a readable premultiplied-BGRA buffer for the whole canvas (stride =
// width*4). For a raster canvas this is `data` directly (no copy: *owned set
// false). For a GPU canvas it reads the surface back into a freshly malloc'd
// buffer (*owned set true; caller must free). Returns nullptr on failure.
static uint8_t *canvas_readable_pixels(nx_canvas_t *canvas, bool *owned) {
	*owned = false;
	if (!canvas->gpu)
		return canvas->data;
	if (!canvas->surface || canvas->width == 0 || canvas->height == 0)
		return nullptr;
	size_t stride = (size_t)canvas->width * 4;
	uint8_t *buf = (uint8_t *)malloc(stride * canvas->height);
	if (!buf)
		return nullptr;
	SkImageInfo info =
	    SkImageInfo::Make(canvas->width, canvas->height,
	                      kBGRA_8888_SkColorType, kPremul_SkAlphaType);
	if (!canvas->surface->readPixels(info, buf, stride, 0, 0)) {
		free(buf);
		return nullptr;
	}
	*owned = true;
	return buf;
}

// ---------------------------------------------------------------------------
// Gradient lifecycle
// ---------------------------------------------------------------------------
void free_gradient(nx_canvas_gradient_t *g) { delete g; }

// ---------------------------------------------------------------------------
// State stack lifecycle (C++ new/delete: state holds sk_sp + std::vector).
// ---------------------------------------------------------------------------

// Apply the default (initial) values to a freshly-created state node.
static void init_state_defaults(nx_canvas_context_2d_state_t *state) {
	state->fill = (nx_rgba_t){0., 0., 0., 1.};
	state->stroke = (nx_rgba_t){0., 0., 0., 1.};
	state->fill_source_type = SOURCE_RGBA;
	state->stroke_source_type = SOURCE_RGBA;
	state->fill_gradient = nullptr;
	state->stroke_gradient = nullptr;
	state->image_smoothing_quality = IMAGE_SMOOTHING_LOW;
	state->image_smoothing_enabled = true;
	state->font_face = nullptr;
	state->font_size = 10.;
	state->font_string = nullptr;
	state->text_baseline = TEXT_BASELINE_ALPHABETIC;
	state->text_align = TEXT_ALIGN_START;
	state->ft_face = nullptr;
	state->hb_font = nullptr;
	state->global_alpha = 1.;
	state->shadow_color = (nx_rgba_t){0., 0., 0., 0.};
	state->shadow_blur = 0.;
	state->shadow_offset_x = 0.;
	state->shadow_offset_y = 0.;
	state->line_width = 1.;
	state->line_cap = SkPaint::kButt_Cap;
	state->line_join = SkPaint::kMiter_Join;
	state->miter_limit = 10.;
	state->line_dash_offset = 0.;
	state->blend_mode = SkBlendMode::kSrcOver;
	state->fill_rule = SkPathFillType::kWinding;
	state->next = nullptr;
}

// Free one state node's owned resources (not the linked list).
static void free_state_node(nx_canvas_context_2d_state_t *s) {
	if (s->font_string)
		free((void *)s->font_string);
	delete s;  // runs sk_sp / std::vector destructors
}

void free_context_state(nx_canvas_context_2d_state_t *state) {
	while (state) {
		nx_canvas_context_2d_state_t *next = state->next;
		free_state_node(state);
		state = next;
	}
}

// ---------------------------------------------------------------------------
// Per-call context setup (backs ENTER_THIS / ENTER_ARGV0).
// ---------------------------------------------------------------------------
struct Ctx2D {
	nx_canvas_context_2d_t *context;
	SkCanvas *cr;
	bool ok;    // surface ready, proceed
	bool noop;  // zero-dim canvas: return without drawing, no exception
};

Ctx2D enter_ctx(Isolate *iso, Local<Value> recv) {
	Ctx2D r = {nullptr, nullptr, false, false};
	r.context = nx::Unwrap<nx_canvas_context_2d_t>(recv);
	if (!r.context)
		return r;
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

#define ENTER_THIS                                                             \
	Isolate *iso = info.GetIsolate();                                          \
	Ctx2D _c = enter_ctx(iso, info.This());                                    \
	if (!_c.ok) {                                                              \
		if (!_c.noop && !_c.context)                                           \
			nx_throw(iso, "invalid canvas context");                           \
		return;                                                                \
	}                                                                          \
	nx_canvas_context_2d_t *context = _c.context;                              \
	SkCanvas *cr = _c.cr;                                                      \
	(void)context;                                                             \
	(void)cr;

#define ENTER_ARGV0                                                            \
	Isolate *iso = info.GetIsolate();                                          \
	Ctx2D _c = enter_ctx(iso, info[0]);                                        \
	if (!_c.ok) {                                                              \
		if (!_c.noop && !_c.context)                                           \
			nx_throw(iso, "invalid canvas context");                           \
		return;                                                                \
	}                                                                          \
	nx_canvas_context_2d_t *context = _c.context;                              \
	SkCanvas *cr = _c.cr;                                                      \
	(void)context;                                                             \
	(void)cr;

// Read `n` doubles from info[offset..]. Returns false (throwing) on failure.
bool get_doubles(const FunctionCallbackInfo<Value> &info, double *args, int n,
                 int offset) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	for (int i = 0; i < n; i++) {
		if (!info[offset + i]->NumberValue(ctx).To(&args[i])) {
			return false;
		}
	}
	return true;
}

// ===========================================================================
// ensure_surface
// ===========================================================================
static void nx_canvas_ensure_surface(Isolate *iso,
                                     nx_canvas_context_2d_t *context) {
	nx_canvas_t *canvas = context->canvas;
	// GPU-backed (screen) canvas: the surface + ctx are owned/managed by the
	// GPU module (FBO 0), not recreated here. Just ensure ctx is wired.
	if (canvas->gpu) {
		if (canvas->surface)
			context->ctx = canvas->surface->getCanvas();
		canvas->surface_dirty = false;
		return;
	}
	if (!canvas->surface_dirty)
		return;

	context->ctx = nullptr;
	canvas->surface.reset();
	if (canvas->data) {
		free(canvas->data);
		canvas->data = nullptr;
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
		canvas->surface =
		    make_raster_surface(buffer, canvas->width, canvas->height);
		if (!canvas->surface) {
			nx_throw(iso, "Failed to create Skia surface");
			free(canvas->data);
			canvas->data = nullptr;
			return;
		}
		context->ctx = canvas->surface->getCanvas();
		canvas->surface_dirty = false;
	}

reset_state:
	context->path.reset();
	// Collapse the state stack to a single default node.
	if (context->state) {
		free_context_state(context->state);
	}
	nx_canvas_context_2d_state_t *state = new nx_canvas_context_2d_state_t();
	init_state_defaults(state);
	context->state = state;
	if (context->default_font_face) {
		nx_font_face_t *face = context->default_font_face;
		state->font_face = face;
		state->ft_face = face->ft_face;
		state->hb_font = face->hb_font;
		state->font_string = strdup("10px sans-serif");
		set_font_size(context, 10.);
	}
}

// ===========================================================================
// set_font_size — keep FT + HarfBuzz scales in sync (Skia size set per-draw).
// ===========================================================================
static void set_font_size(nx_canvas_context_2d_t *context, double font_size) {
	if (!context->state->ft_face || !context->state->hb_font)
		return;
	FT_Set_Char_Size(context->state->ft_face, 0, font_size * 64.0, 0, 0);
	hb_font_set_scale(context->state->hb_font, font_size * 64, font_size * 64);
}

// Shape `text` and return the scale factor to fit within max_width (<= 1).
static double get_text_scale(nx_canvas_context_2d_t *context, const char *text,
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
		width += glyph_pos[i].x_advance / 64.0;
	hb_buffer_destroy(buf);
	return width > max_width ? max_width / width : 1.;
}

// Shape `text` via HarfBuzz and produce Skia glyph IDs + baseline-relative
// positions (y-down), applying text-align and text-baseline offsets. Returns
// the laid-out glyph ids/positions through the out vectors.
static void layout_glyphs(nx_canvas_context_2d_t *context, const char *text,
                          double ox, double oy,
                          std::vector<SkGlyphID> &out_glyphs,
                          std::vector<SkPoint> &out_pos) {
	hb_buffer_t *buf = hb_buffer_create();
	hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
	hb_buffer_set_script(buf, HB_SCRIPT_COMMON);
	hb_buffer_set_language(buf, hb_language_get_default());
	hb_buffer_add_utf8(buf, text, -1, 0, -1);
	hb_shape(context->state->hb_font, buf, NULL, 0);
	unsigned int glyph_count = hb_buffer_get_length(buf);
	hb_glyph_info_t *gi = hb_buffer_get_glyph_infos(buf, NULL);
	hb_glyph_position_t *gp = hb_buffer_get_glyph_positions(buf, NULL);
	out_glyphs.resize(glyph_count);
	out_pos.resize(glyph_count);
	double x = 0, y = 0;
	for (unsigned int i = 0; i < glyph_count; ++i) {
		out_glyphs[i] = (SkGlyphID)gi[i].codepoint;
		// Skia text space is y-down (no negation, unlike the Cairo path).
		out_pos[i] = SkPoint::Make((SkScalar)(x + gp[i].x_offset / 64.0),
		                           (SkScalar)(y + gp[i].y_offset / 64.0));
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
		out_pos[i].offset((SkScalar)(ox + alignment_offset),
		                  (SkScalar)(oy + baseline_offset));
	}
	hb_buffer_destroy(buf);
}

// Build an SkFont for the current state (typeface + size).
static SkFont current_font(nx_canvas_context_2d_t *context, double size) {
	sk_sp<SkTypeface> tf =
	    context->state->font_face ? context->state->font_face->sk_typeface
	                              : nullptr;
	SkFont f(tf, (SkScalar)size);
	f.setSubpixel(true);
	f.setEdging(SkFont::Edging::kAntiAlias);
	return f;
}

// ---------------------------------------------------------------------------
// Path helpers — context->path is stored in DEVICE space: each segment is
// transformed by the current transformation matrix (CTM) at the time it is
// added, per the HTML Canvas spec ("the current path ... in the coordinate
// space described by the current transformation matrix at the time the points
// were added"). Fill/clip then draw the device-space path under an identity
// matrix; stroke maps it back through the stroke-time CTM so the pen scales
// (Chrome behavior, verified). The "current point" is SkPath's last point
// (already in device space).
// ---------------------------------------------------------------------------

// Current transformation matrix (device <- user).
static inline SkMatrix cur_ctm(nx_canvas_context_2d_t *context) {
	return context->ctx->getTotalMatrix();
}

// Map a single user-space point to device space via the current CTM.
static inline SkPoint ctm_pt(nx_canvas_context_2d_t *context, double x,
                             double y) {
	return cur_ctm(context).mapPoint(
	    SkPoint::Make((SkScalar)x, (SkScalar)y));
}

// Append a user-space sub-path to context->path, transformed into device space
// by the current CTM. kExtend connects it to the current point (also device
// space); kAppend starts a new contour.
static inline void add_user_subpath(nx_canvas_context_2d_t *context,
                                    const SkPath &user_sub, bool extend) {
	context->path.addPath(user_sub, cur_ctm(context),
	                      extend ? SkPath::kExtend_AddPathMode
	                             : SkPath::kAppend_AddPathMode);
}

// Whether the path has a current point (an open contour to continue from).
static bool path_has_current(const SkPathBuilder &p, SkPoint *out) {
	if (p.countPoints() == 0)
		return false;
	std::optional<SkPoint> lp = p.getLastPt();
	if (!lp)
		return false;
	if (out)
		*out = *lp;
	return true;
}

// ---------------------------------------------------------------------------
// save_path / restore_path — temporarily stash the user's current path while a
// rect/Path2D primitive is drawn, then restore it.
// ---------------------------------------------------------------------------
static SkPath g_saved_path_unused;  // (path is stored on a local in callers)

// Whether a shadow is active (matches Cairo has_shadow).
static bool has_shadow(nx_canvas_context_2d_t *context) {
	nx_canvas_context_2d_state_t *s = context->state;
	return s->shadow_color.a > 0 &&
	       (s->shadow_blur > 0 || s->shadow_offset_x != 0 ||
	        s->shadow_offset_y != 0);
}

// Build a DropShadow image filter for the current shadow state, or null. The
// Canvas spec maps shadowBlur to a Gaussian sigma = shadowBlur / 2. The shadow
// color includes global_alpha (matching the Cairo path). Attaching this to the
// draw paint renders the shape AND its shadow in one pass.
static sk_sp<SkImageFilter> shadow_filter(nx_canvas_context_2d_t *context) {
	if (!has_shadow(context))
		return nullptr;
	nx_canvas_context_2d_state_t *s = context->state;
	SkScalar sigma = (SkScalar)(s->shadow_blur / 2.0);
	SkColor color = rgba_to_color(s->shadow_color, s->global_alpha);
	return SkImageFilters::DropShadow((SkScalar)s->shadow_offset_x,
	                                  (SkScalar)s->shadow_offset_y, sigma, sigma,
	                                  color, nullptr);
}

// Paint construction
static SkPaint make_fill_paint(nx_canvas_context_2d_t *context) {
	nx_canvas_context_2d_state_t *s = context->state;
	SkPaint p;
	p.setAntiAlias(true);
	p.setStyle(SkPaint::kFill_Style);
	p.setBlendMode(s->blend_mode);
	if (s->fill_source_type == SOURCE_GRADIENT && s->fill_gradient &&
	    s->fill_gradient->shader) {
		p.setShader(s->fill_gradient->shader);
		p.setAlphaf((float)s->global_alpha);
	} else {
		p.setColor(rgba_to_color(s->fill, s->global_alpha));
	}
	p.setImageFilter(shadow_filter(context));
	return p;
}

static void apply_stroke_style(nx_canvas_context_2d_t *context, SkPaint &p) {
	nx_canvas_context_2d_state_t *s = context->state;
	p.setStyle(SkPaint::kStroke_Style);
	p.setStrokeWidth((SkScalar)s->line_width);
	p.setStrokeCap(s->line_cap);
	p.setStrokeJoin(s->line_join);
	p.setStrokeMiter((SkScalar)s->miter_limit);
	if (!s->line_dash.empty()) {
		p.setPathEffect(SkDashPathEffect::Make(
		    SkSpan<const SkScalar>(s->line_dash.data(), s->line_dash.size()),
		    (SkScalar)s->line_dash_offset));
	}
}

static SkPaint make_stroke_paint(nx_canvas_context_2d_t *context) {
	nx_canvas_context_2d_state_t *s = context->state;
	SkPaint p;
	p.setAntiAlias(true);
	p.setBlendMode(s->blend_mode);
	apply_stroke_style(context, p);
	if (s->stroke_source_type == SOURCE_GRADIENT && s->stroke_gradient &&
	    s->stroke_gradient->shader) {
		p.setShader(s->stroke_gradient->shader);
		p.setAlphaf((float)s->global_alpha);
	} else {
		p.setColor(rgba_to_color(s->stroke, s->global_alpha));
	}
	p.setImageFilter(shadow_filter(context));
	return p;
}

// fill_op / stroke_op: context->path is in DEVICE space (the CTM was baked in
// at build time), so draw it under an identity matrix.
//
// Fill: just reset the matrix around the draw.
//
// Stroke: the pen must still be scaled/sheared by the stroke-time CTM (a
// horizontal line under scale(1,3) is 3x thicker — verified against Chrome).
// So map the device path back into user space via the inverse CTM and stroke
// it under the CTM, which applies the pen in user space and transforms the
// result — reproducing the elliptical pen.
static void fill_op(nx_canvas_context_2d_t *context, bool /*preserve*/) {
	context->path.setFillType(context->state->fill_rule);
	SkPaint p = make_fill_paint(context);
	SkCanvas *cr = context->ctx;
	SkM44 saved = cr->getLocalToDevice();
	cr->resetMatrix();
	cr->drawPath(context->path.snapshot(), p);
	cr->setMatrix(saved);
}
static void stroke_op(nx_canvas_context_2d_t *context, bool /*preserve*/) {
	SkPaint p = make_stroke_paint(context);
	SkCanvas *cr = context->ctx;
	SkMatrix ctm = cr->getTotalMatrix();
	SkMatrix inv;
	if (ctm.invert(&inv)) {
		// Path back to user space; stroke under the existing CTM so the pen is
		// transformed by it.
		SkPath user = context->path.snapshot().makeTransform(inv);
		cr->drawPath(user, p);
	} else {
		// Degenerate CTM: fall back to stroking the device path under identity.
		SkM44 saved = cr->getLocalToDevice();
		cr->resetMatrix();
		cr->drawPath(context->path.snapshot(), p);
		cr->setMatrix(saved);
	}
}

// Set the path fill rule from a JS string ("evenodd" -> EvenOdd).
static void set_fill_rule_v(Isolate *iso, Local<Value> fill_rule,
                            nx_canvas_context_2d_t *context) {
	SkPathFillType rule = SkPathFillType::kWinding;
	if (fill_rule->IsString()) {
		String::Utf8Value str(iso, fill_rule);
		if (*str && strcmp(*str, "evenodd") == 0)
			rule = SkPathFillType::kEvenOdd;
	}
	context->state->fill_rule = rule;
	context->path.setFillType(rule);
}

// Apply a user Path2D onto context->path. Path2D is backed by an SkPath in
// USER space (source/path2d.cc); bake the current CTM into it so context->path
// stays in device space (consistent with the inline path builders).
void apply_path(Isolate *iso, Local<Value> recv, Local<Value> path) {
	(void)recv;
	const SkPath *user = nx_path2d_get(iso, path);
	if (!user)
		return;
	nx_canvas_context_2d_t *context = nx_get_canvas_context_2d(iso, recv);
	if (!context)
		return;
	context->path.addPath(*user, context->ctx->getTotalMatrix(),
	                      SkPath::kAppend_AddPathMode);
}

// Resolve the WebIDL overloads shared by fill()/clip(): `(fillRule?)` and
// `(Path2D path, fillRule?)`. Overload selection is by ARGUMENT COUNT (matching
// Chrome): with 2+ arguments the Path2D overload is chosen, so arg0 must be a
// Path2D (else TypeError) and arg1 is the optional fill rule; with 0-1
// arguments the no-path overload is chosen, so arg0 (if present) is the fill
// rule string (a non-string, non-undefined arg0 is a TypeError). A non-Path2D
// object in the path position throws rather than silently becoming an empty
// path. Returns false (and throws) on a type error; on success sets out_path /
// out_rule (each may stay empty). Mirrors fill()/clip()/Path2D semantics so the
// behavior is identical across the three.
static bool resolve_path_and_rule(const FunctionCallbackInfo<Value> &info,
                                   Local<Value> *out_path, bool *out_have_path,
                                   Local<Value> *out_rule, bool *out_have_rule) {
	Isolate *iso = info.GetIsolate();
	*out_have_path = false;
	*out_have_rule = false;
	if (info.Length() >= 2) {
		// (path, fillRule?) overload — arg0 MUST be a Path2D.
		if (!nx_path2d_get(iso, info[0])) {
			nx_throw(iso, "Expected Path2D at index 0");
			return false;
		}
		*out_path = info[0];
		*out_have_path = true;
		if (info[1]->IsString()) {
			*out_rule = info[1];
			*out_have_rule = true;
		} else if (!info[1]->IsUndefined()) {
			nx_throw(iso, "Expected string at index 1");
			return false;
		}
		// Trailing arguments (index >= 2) are ignored, per WebIDL.
	} else if (info.Length() == 1) {
		if (info[0]->IsObject()) {
			// An object in the single-arg position selects the path overload;
			// it must be a real Path2D.
			if (!nx_path2d_get(iso, info[0])) {
				nx_throw(iso, "Expected Path2D at index 0");
				return false;
			}
			*out_path = info[0];
			*out_have_path = true;
		} else if (info[0]->IsString()) {
			*out_rule = info[0];
			*out_have_rule = true;
		} else if (!info[0]->IsUndefined()) {
			nx_throw(iso, "Expected Path2D or string at index 0");
			return false;
		}
	}
	return true;
}

// ===========================================================================
// STUBS — filled in subsequent slices (primitives, paint, transforms, text,
// images, imageData, encode). Each throws so a partial build is obvious.
// ===========================================================================
#define STUB(NAME)                                                             \
	void NAME(const FunctionCallbackInfo<Value> &info) {                       \
		nx_throw(info.GetIsolate(), #NAME " not implemented yet (Skia port)"); \
	}

#undef STUB

// Forward declarations for slice-6 functions defined after the registration
// block (which references them).
void nx_canvas_context_2d_is_point_in_path(
    const FunctionCallbackInfo<Value> &info);
void nx_canvas_context_2d_is_point_in_stroke(
    const FunctionCallbackInfo<Value> &info);
void nx_canvas_context_2d_put_image_data(
    const FunctionCallbackInfo<Value> &info);
void nx_canvas_context_2d_get_image_data(
    const FunctionCallbackInfo<Value> &info);
void nx_canvas_context_2d_draw_image(const FunctionCallbackInfo<Value> &info);
void nx_canvas_context_2d_set_fill_style_gradient(
    const FunctionCallbackInfo<Value> &info);
void nx_canvas_context_2d_set_stroke_style_gradient(
    const FunctionCallbackInfo<Value> &info);
void nx_canvas_gradient_new_linear(const FunctionCallbackInfo<Value> &info);
void nx_canvas_gradient_new_radial(const FunctionCallbackInfo<Value> &info);
void nx_canvas_gradient_add_color_stop(
    const FunctionCallbackInfo<Value> &info);
void nx_canvas_proto_to_data_url(const FunctionCallbackInfo<Value> &info);
void nx_canvas_to_buffer(const FunctionCallbackInfo<Value> &info);

// ===========================================================================
// Slice 3: path primitives + transforms
// ===========================================================================

void nx_canvas_context_2d_move_to(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double a[2];
	if (!get_doubles(info, a, 2, 0))
		return;
	SkPoint p = ctm_pt(context, a[0], a[1]);
	context->path.moveTo(p);
}

void nx_canvas_context_2d_line_to(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double a[2];
	if (!get_doubles(info, a, 2, 0))
		return;
	SkPoint p = ctm_pt(context, a[0], a[1]);
	if (!path_has_current(context->path, nullptr))
		context->path.moveTo(p);
	else
		context->path.lineTo(p);
}

void nx_canvas_context_2d_bezier_curve_to(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double a[6];
	if (!get_doubles(info, a, 6, 0))
		return;
	SkPoint c1 = ctm_pt(context, a[0], a[1]);
	SkPoint c2 = ctm_pt(context, a[2], a[3]);
	SkPoint end = ctm_pt(context, a[4], a[5]);
	if (!path_has_current(context->path, nullptr))
		context->path.moveTo(c1);
	context->path.cubicTo(c1, c2, end);
}

void nx_canvas_context_2d_quadratic_curve_to(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double a[4];
	if (!get_doubles(info, a, 4, 0))
		return;
	SkPoint cp = ctm_pt(context, a[0], a[1]);
	SkPoint end = ctm_pt(context, a[2], a[3]);
	if (!path_has_current(context->path, nullptr))
		context->path.moveTo(cp);
	context->path.quadTo(cp, end);
}

void nx_canvas_context_2d_arc(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double a[5];
	if (!get_doubles(info, a, 5, 0))
		return;
	double x = a[0], y = a[1], radius = a[2], startAngle = a[3],
	       endAngle = a[4];
	if (radius < 0) {
		iso->ThrowException(Exception::RangeError(
		    nx_str(iso, "The radius provided is negative.")));
		return;
	}
	bool ccw = info.Length() > 5 ? info[5]->BooleanValue(iso) : false;
	// Build the arc in user space (seeded with the current point mapped back
	// into user space so the connecting line lands correctly), then bake the
	// CTM into device space.
	SkPathBuilder sub;
	SkPoint cur;
	bool had_current = path_has_current(context->path, &cur);
	if (had_current) {
		SkMatrix inv;
		if (cur_ctm(context).invert(&inv))
			sub.moveTo(inv.mapPoint(cur));
	}
	nxcp_arc(sub, x, y, radius, startAngle, endAngle, ccw);
	add_user_subpath(context, sub.detach(), had_current);
}

void nx_canvas_context_2d_arc_to(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double a[5];
	if (!get_doubles(info, a, 5, 0))
		return;
	// arcTo math runs in user space; the current point (device space) is mapped
	// back through the CTM. The resulting sub-path is then baked to device
	// space via add_user_subpath. `sub` is seeded with the user-space current
	// point so path_arc's connecting line lands correctly.
	SkMatrix ctm = cur_ctm(context);
	SkMatrix inv;
	bool have_inv = ctm.invert(&inv);
	SkPoint p0pt_dev;
	bool had_current = path_has_current(context->path, &p0pt_dev);
	SkPoint p0pt = (had_current && have_inv) ? inv.mapPoint(p0pt_dev)
	                                         : SkPoint::Make(0, 0);
	SkPathBuilder sub;
	if (had_current)
		sub.moveTo(p0pt);
	nxcp_arc_to(sub, p0pt, a[0], a[1], a[2], a[3], a[4]);
	add_user_subpath(context, sub.detach(), had_current);
}

void nx_canvas_context_2d_ellipse(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double a[7];
	if (!get_doubles(info, a, 7, 0))
		return;
	double radiusX = a[2], radiusY = a[3];
	if (radiusX == 0 || radiusY == 0)
		return;
	double x = a[0], y = a[1], rotation = a[4], startAngle = a[5],
	       endAngle = a[6];
	bool anticlockwise =
	    info.Length() >= 8 ? info[7]->BooleanValue(iso) : false;
	// Build the ellipse arc in user (LOCAL) space via nxcp_ellipse, then bake
	// the CTM into device space via add_user_subpath. The current point (device
	// space) is mapped back through the CTM into LOCAL space so the connecting
	// line lands correctly after transform.
	SkPoint cur;
	bool had_current = path_has_current(context->path, &cur);
	// Only seed the connecting line when the current point can be mapped back
	// into LOCAL space (CTM invertible). If the CTM is singular, leave the
	// sub-path unseeded so nxcp_ellipse's arc starts a fresh contour — matching
	// the original device-space behavior exactly.
	SkPoint p0_local = SkPoint::Make(0, 0);
	bool seed = false;
	if (had_current) {
		SkMatrix inv;
		if (cur_ctm(context).invert(&inv)) {
			p0_local = inv.mapPoint(cur);
			seed = true;
		}
	}
	SkPathBuilder sub;
	nxcp_ellipse(sub, x, y, radiusX, radiusY, rotation, startAngle, endAngle,
	             anticlockwise, seed, p0_local);
	add_user_subpath(context, sub.detach(), had_current);
}

void nx_canvas_context_2d_rect(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double a[4];
	if (!get_doubles(info, a, 4, 0))
		return;
	double x = a[0], y = a[1], width = a[2], height = a[3];
	// Build in user space, then bake the CTM into the points (device space).
	// rect() always starts a new subpath, so append (don't extend).
	SkPathBuilder sub;
	nxcp_rect(sub, x, y, width, height);
	add_user_subpath(context, sub.detach(), /*extend=*/false);
}

// Returns true (throwing) on error.
static bool get_radius(Isolate *iso, Local<Value> v, Point *p) {
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

// Shared by ctx.roundRect and Path2D.roundRect (definition is at global scope
// below, outside this anonymous namespace, so path2d.cc can link to it).
bool resolve_round_rect_radii_impl(Isolate *iso, Local<Value> radii,
                                   SkVector out[4]) {
	Local<Context> jsctx = iso->GetCurrentContext();
	Point r[4];
	uint32_t nRadii = 4;
	if (radii.IsEmpty() || radii->IsUndefined()) {
		for (int i = 0; i < 4; i++)
			r[i].x = r[i].y = 0.;
	} else if (radii->IsArray()) {
		Local<Object> arr = radii.As<Object>();
		Local<Value> length_val;
		if (!arr->Get(jsctx, nx_str(iso, "length")).ToLocal(&length_val) ||
		    !length_val->Uint32Value(jsctx).To(&nRadii))
			return false;
		if (!(nRadii >= 1 && nRadii <= 4)) {
			iso->ThrowException(Exception::RangeError(nx_str(
			    iso,
			    "radii must be a list of one, two, three or four radii.")));
			return false;
		}
		for (uint32_t i = 0; i < nRadii; i++) {
			Local<Value> v;
			if (!arr->Get(jsctx, i).ToLocal(&v))
				return false;
			if (get_radius(iso, v, &r[i]))
				return false;
		}
	} else {
		if (get_radius(iso, radii, &r[0]))
			return false;
		for (int i = 1; i < 4; i++) {
			r[i].x = r[0].x;
			r[i].y = r[0].y;
		}
	}
	Point ul, ur, lr, ll;
	if (nRadii == 4) {
		ul = r[0]; ur = r[1]; lr = r[2]; ll = r[3];
	} else if (nRadii == 3) {
		ul = r[0]; ur = r[1]; ll = r[1]; lr = r[2];
	} else if (nRadii == 2) {
		ul = r[0]; lr = r[0]; ur = r[1]; ll = r[1];
	} else {
		ul = ur = lr = ll = r[0];
	}
	out[0] = {(SkScalar)ul.x, (SkScalar)ul.y};
	out[1] = {(SkScalar)ur.x, (SkScalar)ur.y};
	out[2] = {(SkScalar)lr.x, (SkScalar)lr.y};
	out[3] = {(SkScalar)ll.x, (SkScalar)ll.y};
	return true;
}

void nx_canvas_context_2d_round_rect(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	Local<Context> jsctx = iso->GetCurrentContext();
	double rargs[4];
	if (!get_doubles(info, rargs, 4, 0))
		return;
	(void)jsctx;
	double x = rargs[0], y = rargs[1], width = rargs[2], height = rargs[3];
	// Radii in SkRRect order (TL, TR, BR, BL).
	SkVector radii[4];
	if (!nx_resolve_round_rect_radii(
	        iso, info.Length() > 4 ? info[4] : Local<Value>(), radii))
		return;
	bool clockwise = true;
	// Match the prior behavior exactly: each negative dimension swaps the two
	// left radii with the two right radii (TL<->TR, BL<->BR).
	if (width < 0) {
		clockwise = false;
		x += width;
		width = -width;
		std::swap(radii[0], radii[1]);
		std::swap(radii[3], radii[2]);
	}
	if (height < 0) {
		clockwise = !clockwise;
		y += height;
		height = -height;
		std::swap(radii[0], radii[1]);
		std::swap(radii[3], radii[2]);
	}
	// The spec radii down-scale (clamp) and the rrect build happen in
	// nxcp_round_rect (LOCAL space); the resulting sub-path is then baked into
	// device space (new subpath).
	SkPathBuilder sub;
	nxcp_round_rect(sub, x, y, width, height, radii, clockwise);
	add_user_subpath(context, sub.detach(), /*extend=*/false);
}

void nx_canvas_context_2d_begin_path(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	context->path.reset();
	context->path.setFillType(context->state->fill_rule);
}
void nx_canvas_context_2d_close_path(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	context->path.close();
}

void nx_canvas_context_2d_clip(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	// clip() has two WebIDL overloads — clip(fillRule?) (clip against the
	// context's current path) and clip(path, fillRule?) (clip against a Path2D)
	// — resolved by argument count + arg0 type (see resolve_path_and_rule).
	Local<Value> path, fill_rule;
	bool have_path = false, have_rule = false;
	if (!resolve_path_and_rule(info, &path, &have_path, &fill_rule, &have_rule))
		return; // a TypeError was thrown

	// When a Path2D is given, clip against THAT path (baked into device space
	// via apply_path, like fill(path)) instead of the context's current path,
	// without disturbing the current path. The fill rule (winding/evenodd)
	// applies to whichever path is used.
	SkPathBuilder saved_path;
	bool restore_path = false;
	if (have_path) {
		saved_path = context->path;
		restore_path = true;
		context->path.reset();
		apply_path(iso, info.This(), path);
	}
	if (have_rule)
		set_fill_rule_v(iso, fill_rule, context);

	// context->path is in device space; clip under identity so clipPath does
	// not re-apply the CTM. Toggle the matrix directly (NOT via save/restore,
	// which would also pop the clip we are adding).
	SkM44 saved = cr->getLocalToDevice();
	cr->resetMatrix();
	cr->clipPath(context->path.snapshot(), true);
	cr->setMatrix(saved);

	if (restore_path)
		context->path = saved_path;
}

void nx_canvas_context_2d_fill(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	// fill(fillRule?) / fill(path, fillRule?) — same overload resolution as
	// clip() (by arg count + arg0 type); a non-Path2D object in the path
	// position throws rather than silently filling an empty path.
	Local<Value> path, fill_rule;
	bool have_path = false, have_rule = false;
	if (!resolve_path_and_rule(info, &path, &have_path, &fill_rule, &have_rule))
		return; // a TypeError was thrown
	if (have_rule)
		set_fill_rule_v(iso, fill_rule, context);
	if (!have_path) {
		fill_op(context, true);
	} else {
		SkPathBuilder saved = context->path;
		context->path.reset();
		apply_path(iso, info.This(), path);
		fill_op(context, false);
		context->path = saved;
	}
}

void nx_canvas_context_2d_stroke(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	// stroke() / stroke(path) — the single overload's optional argument is a
	// Path2D. With an argument present it MUST be a real Path2D (a non-Path2D
	// value, object or otherwise, is a TypeError) so we never stroke an empty
	// path silently; trailing arguments are ignored.
	Local<Value> path;
	bool have_path = false;
	if (info.Length() >= 1 && !info[0]->IsUndefined()) {
		if (!nx_path2d_get(iso, info[0])) {
			nx_throw(iso, "Expected Path2D at index 0");
			return;
		}
		path = info[0];
		have_path = true;
	}
	if (!have_path) {
		stroke_op(context, true);
	} else {
		SkPathBuilder saved = context->path;
		context->path.reset();
		apply_path(iso, info.This(), path);
		stroke_op(context, false);
		context->path = saved;
	}
}

void nx_canvas_context_2d_fill_rect(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double a[4];
	if (!get_doubles(info, a, 4, 0))
		return;
	if (a[2] && a[3]) {
		SkPaint p = make_fill_paint(context);
		cr->drawRect(SkRect::MakeXYWH((SkScalar)a[0], (SkScalar)a[1],
		                              (SkScalar)a[2], (SkScalar)a[3]),
		             p);
	}
}

void nx_canvas_context_2d_stroke_rect(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double a[4];
	if (!get_doubles(info, a, 4, 0))
		return;
	if (a[2] && a[3]) {
		SkPaint p = make_stroke_paint(context);
		cr->drawRect(SkRect::MakeXYWH((SkScalar)a[0], (SkScalar)a[1],
		                              (SkScalar)a[2], (SkScalar)a[3]),
		             p);
	}
}

void nx_canvas_context_2d_clear_rect(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double a[4];
	if (!get_doubles(info, a, 4, 0))
		return;
	if (a[2] && a[3]) {
		SkPaint p;
		p.setBlendMode(SkBlendMode::kClear);
		cr->drawRect(SkRect::MakeXYWH((SkScalar)a[0], (SkScalar)a[1],
		                              (SkScalar)a[2], (SkScalar)a[3]),
		             p);
	}
}

// ---- transforms ----
void nx_canvas_context_2d_rotate(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double a[1];
	if (!get_doubles(info, a, 1, 0))
		return;
	cr->rotate((SkScalar)(a[0] * 180.0 / M_PI));
}
void nx_canvas_context_2d_scale(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double a[2];
	if (!get_doubles(info, a, 2, 0))
		return;
	cr->scale((SkScalar)a[0], (SkScalar)a[1]);
}
void nx_canvas_context_2d_translate(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double a[2];
	if (!get_doubles(info, a, 2, 0))
		return;
	cr->translate((SkScalar)a[0], (SkScalar)a[1]);
}
void nx_canvas_context_2d_transform(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double a[6];
	if (!get_doubles(info, a, 6, 0))
		return;
	// CSS/Cairo order: a,b,c,d,e,f -> [a c e; b d f]. SkMatrix is row-major
	// [sx kx tx; ky sy ty; 0 0 1].
	SkMatrix m;
	m.setAll((SkScalar)a[0], (SkScalar)a[2], (SkScalar)a[4], (SkScalar)a[1],
	         (SkScalar)a[3], (SkScalar)a[5], 0, 0, 1);
	cr->concat(m);
}
void nx_canvas_context_2d_reset_transform(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	cr->resetMatrix();
}
void nx_canvas_context_2d_set_transform(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	cr->resetMatrix();
	if (info.Length() == 0 || info[0]->IsUndefined())
		return;
	double a[6];
	if (info.Length() >= 6) {
		if (!get_doubles(info, a, 6, 0))
			return;
	} else if (info[0]->IsObject()) {
		// DOMMatrix-like object.
		nx_dommatrix_t dm;
		if (nx_dommatrix_init(iso, info[0], &dm) != 0)
			return;
		a[0] = dm.values.m11;
		a[1] = dm.values.m12;
		a[2] = dm.values.m21;
		a[3] = dm.values.m22;
		a[4] = dm.values.m41;
		a[5] = dm.values.m42;
	} else {
		return;
	}
	SkMatrix m;
	m.setAll((SkScalar)a[0], (SkScalar)a[2], (SkScalar)a[4], (SkScalar)a[1],
	         (SkScalar)a[3], (SkScalar)a[5], 0, 0, 1);
	cr->setMatrix(SkM44(m));
}

// ===========================================================================
// Slice 4: paint-state properties (fill/stroke style, line styling, alpha,
// composite op, shadow, smoothing, font, text align/baseline).
// ===========================================================================

// ---- fillStyle / strokeStyle (solid color; rgba 0..255 in, stored 0..1) ----
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
	double a[4];
	if (!get_doubles(info, a, 4, 1))
		return;
	context->state->fill.r = a[0] / 255.;
	context->state->fill.g = a[1] / 255.;
	context->state->fill.b = a[2] / 255.;
	context->state->fill.a = a[3];
	context->state->fill_source_type = SOURCE_RGBA;
	context->state->fill_gradient = nullptr;
}
void nx_canvas_context_2d_get_stroke_style(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_ARGV0;
	Local<Context> jsctx = iso->GetCurrentContext();
	Local<Array> rgba = Array::New(iso, 4);
	rgba->Set(jsctx, 0, Integer::New(iso, context->state->stroke.r * 255))
	    .Check();
	rgba->Set(jsctx, 1, Integer::New(iso, context->state->stroke.g * 255))
	    .Check();
	rgba->Set(jsctx, 2, Integer::New(iso, context->state->stroke.b * 255))
	    .Check();
	rgba->Set(jsctx, 3, Number::New(iso, context->state->stroke.a)).Check();
	info.GetReturnValue().Set(rgba);
}
void nx_canvas_context_2d_set_stroke_style(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_ARGV0;
	double a[4];
	if (!get_doubles(info, a, 4, 1))
		return;
	context->state->stroke.r = a[0] / 255.;
	context->state->stroke.g = a[1] / 255.;
	context->state->stroke.b = a[2] / 255.;
	context->state->stroke.a = a[3];
	context->state->stroke_source_type = SOURCE_RGBA;
	context->state->stroke_gradient = nullptr;
}

// ---- font ----
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
	set_font_size(context, font_size);
	if (!context->default_font_face)
		context->default_font_face = face;
}

// ---- transform read-back ----
void nx_canvas_context_2d_get_transform(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_ARGV0;
	Local<Context> jsctx = iso->GetCurrentContext();
	SkMatrix m = cr->getTotalMatrix();
	// CSS order [a b c d e f] = [sx ky kx sy tx ty].
	double v[6] = {m.getScaleX(), m.getSkewY(),  m.getSkewX(),
	               m.getScaleY(), m.getTranslateX(), m.getTranslateY()};
	Local<Array> array = Array::New(iso, 6);
	for (int i = 0; i < 6; i++)
		array->Set(jsctx, i, Number::New(iso, v[i])).Check();
	info.GetReturnValue().Set(array);
}

// ---- line width / join / cap / miter ----
void nx_canvas_context_2d_get_line_width(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	info.GetReturnValue().Set(Number::New(iso, context->state->line_width));
}
void nx_canvas_context_2d_set_line_width(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double n;
	if (!info[0]->NumberValue(iso->GetCurrentContext()).To(&n))
		return;
	if (n > 0 && isfinite(n))
		context->state->line_width = n;
}
void nx_canvas_context_2d_get_line_join(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	const char *join;
	switch (context->state->line_join) {
	case SkPaint::kBevel_Join: join = "bevel"; break;
	case SkPaint::kRound_Join: join = "round"; break;
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
	if (!strcmp("round", *type))
		context->state->line_join = SkPaint::kRound_Join;
	else if (!strcmp("bevel", *type))
		context->state->line_join = SkPaint::kBevel_Join;
	else
		context->state->line_join = SkPaint::kMiter_Join;
}
void nx_canvas_context_2d_get_line_cap(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	const char *cap;
	switch (context->state->line_cap) {
	case SkPaint::kRound_Cap: cap = "round"; break;
	case SkPaint::kSquare_Cap: cap = "square"; break;
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
	if (!strcmp("round", *type))
		context->state->line_cap = SkPaint::kRound_Cap;
	else if (!strcmp("square", *type))
		context->state->line_cap = SkPaint::kSquare_Cap;
	else
		context->state->line_cap = SkPaint::kButt_Cap;
}
void nx_canvas_context_2d_get_miter_limit(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	info.GetReturnValue().Set(Number::New(iso, context->state->miter_limit));
}
void nx_canvas_context_2d_set_miter_limit(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double limit;
	if (!info[0]->NumberValue(iso->GetCurrentContext()).To(&limit))
		return;
	if (limit > 0)
		context->state->miter_limit = limit;
}

// ---- line dash ----
void nx_canvas_context_2d_get_line_dash_offset(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	info.GetReturnValue().Set(
	    Number::New(iso, context->state->line_dash_offset));
}
void nx_canvas_context_2d_set_line_dash_offset(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	double offset;
	if (!info[0]->NumberValue(iso->GetCurrentContext()).To(&offset))
		return;
	context->state->line_dash_offset = offset;
}
void nx_canvas_context_2d_get_line_dash(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	Local<Context> jsctx = iso->GetCurrentContext();
	const std::vector<float> &d = context->state->line_dash;
	Local<Array> array = Array::New(iso, (int)d.size());
	for (size_t i = 0; i < d.size(); i++)
		array->Set(jsctx, (uint32_t)i, Number::New(iso, d[i])).Check();
	info.GetReturnValue().Set(array);
}
void nx_canvas_context_2d_set_line_dash(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	Local<Context> jsctx = iso->GetCurrentContext();
	if (!info[0]->IsObject()) {
		nx_throw(iso, "setLineDash: argument is not a list");
		return;
	}
	Local<Object> arr = info[0].As<Object>();
	uint32_t length;
	Local<Value> length_val;
	if (!arr->Get(jsctx, nx_str(iso, "length")).ToLocal(&length_val) ||
	    !length_val->Uint32Value(jsctx).To(&length))
		return;
	// SkDashPathEffect requires an even, non-empty interval count. CSS doubles
	// an odd-length pattern.
	uint32_t num_dashes = (length & 1) ? length * 2 : length;
	std::vector<float> dashes;
	dashes.reserve(num_dashes);
	uint32_t zero_dashes = 0;
	for (uint32_t i = 0; i < num_dashes; i++) {
		double v;
		Local<Value> val;
		if (!arr->Get(jsctx, i % length).ToLocal(&val))
			return;
		if (!val->NumberValue(jsctx).To(&v))
			return;
		if (v == 0)
			zero_dashes++;
		dashes.push_back((float)v);
	}
	if (num_dashes == 0 || zero_dashes == num_dashes)
		context->state->line_dash.clear();
	else
		context->state->line_dash = std::move(dashes);
}

// ---- global alpha ----
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

// ---- shadow ----
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
#undef SIMPLE_DOUBLE_GETSET

void nx_canvas_context_2d_get_shadow_color(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_ARGV0;
	Local<Context> jsctx = iso->GetCurrentContext();
	Local<Array> arr = Array::New(iso, 4);
	arr->Set(jsctx, 0, Integer::New(iso, context->state->shadow_color.r * 255))
	    .Check();
	arr->Set(jsctx, 1, Integer::New(iso, context->state->shadow_color.g * 255))
	    .Check();
	arr->Set(jsctx, 2, Integer::New(iso, context->state->shadow_color.b * 255))
	    .Check();
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

// ---- image smoothing ----
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
	case IMAGE_SMOOTHING_HIGH: q = "high"; break;
	case IMAGE_SMOOTHING_MEDIUM: q = "medium"; break;
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
		context->state->image_smoothing_quality = IMAGE_SMOOTHING_HIGH;
	else if (!strcmp(*str, "medium"))
		context->state->image_smoothing_quality = IMAGE_SMOOTHING_MEDIUM;
	else if (!strcmp(*str, "low"))
		context->state->image_smoothing_quality = IMAGE_SMOOTHING_LOW;
}

// ---- text align / baseline ----
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
	if (!strcmp(*str, "start"))
		context->state->text_align = TEXT_ALIGN_START;
	else if (!strcmp(*str, "left"))
		context->state->text_align = TEXT_ALIGN_LEFT;
	else if (!strcmp(*str, "center"))
		context->state->text_align = TEXT_ALIGN_CENTER;
	else if (!strcmp(*str, "right"))
		context->state->text_align = TEXT_ALIGN_RIGHT;
	else if (!strcmp(*str, "end"))
		context->state->text_align = TEXT_ALIGN_END;
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
	if (!strcmp(*str, "alphabetic"))
		context->state->text_baseline = TEXT_BASELINE_ALPHABETIC;
	else if (!strcmp(*str, "top"))
		context->state->text_baseline = TEXT_BASELINE_TOP;
	else if (!strcmp(*str, "middle"))
		context->state->text_baseline = TEXT_BASELINE_MIDDLE;
	else if (!strcmp(*str, "bottom"))
		context->state->text_baseline = TEXT_BASELINE_BOTTOM;
	else if (!strcmp(*str, "ideographic"))
		context->state->text_baseline = TEXT_BASELINE_IDEOGRAPHIC;
	else if (!strcmp(*str, "hanging"))
		context->state->text_baseline = TEXT_BASELINE_HANGING;
}

// ---- globalCompositeOperation (CSS string <-> SkBlendMode) ----
void nx_canvas_context_2d_get_global_composite_operation(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	const char *op;
	switch (context->state->blend_mode) {
	case SkBlendMode::kClear: op = "clear"; break;
	case SkBlendMode::kSrc: op = "copy"; break;
	case SkBlendMode::kDst: op = "destination"; break;
	case SkBlendMode::kSrcOver: op = "source-over"; break;
	case SkBlendMode::kDstOver: op = "destination-over"; break;
	case SkBlendMode::kSrcIn: op = "source-in"; break;
	case SkBlendMode::kDstIn: op = "destination-in"; break;
	case SkBlendMode::kSrcOut: op = "source-out"; break;
	case SkBlendMode::kDstOut: op = "destination-out"; break;
	case SkBlendMode::kSrcATop: op = "source-atop"; break;
	case SkBlendMode::kDstATop: op = "destination-atop"; break;
	case SkBlendMode::kXor: op = "xor"; break;
	case SkBlendMode::kPlus: op = "lighter"; break;
	case SkBlendMode::kMultiply: op = "multiply"; break;
	case SkBlendMode::kScreen: op = "screen"; break;
	case SkBlendMode::kOverlay: op = "overlay"; break;
	case SkBlendMode::kDarken: op = "darken"; break;
	case SkBlendMode::kLighten: op = "lighten"; break;
	case SkBlendMode::kColorDodge: op = "color-dodge"; break;
	case SkBlendMode::kColorBurn: op = "color-burn"; break;
	case SkBlendMode::kHardLight: op = "hard-light"; break;
	case SkBlendMode::kSoftLight: op = "soft-light"; break;
	case SkBlendMode::kDifference: op = "difference"; break;
	case SkBlendMode::kExclusion: op = "exclusion"; break;
	case SkBlendMode::kHue: op = "hue"; break;
	case SkBlendMode::kSaturation: op = "saturation"; break;
	case SkBlendMode::kColor: op = "color"; break;
	case SkBlendMode::kLuminosity: op = "luminosity"; break;
	default: op = "source-over";
	}
	info.GetReturnValue().Set(nx_str(iso, op));
}
void nx_canvas_context_2d_set_global_composite_operation(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	String::Utf8Value s(iso, info[0]);
	if (!*s)
		return;
	const char *str = *s;
	SkBlendMode m;
	bool ok = true;
	if (!strcmp(str, "clear")) m = SkBlendMode::kClear;
	else if (!strcmp(str, "copy")) m = SkBlendMode::kSrc;
	else if (!strcmp(str, "destination")) m = SkBlendMode::kDst;
	else if (!strcmp(str, "source-over")) m = SkBlendMode::kSrcOver;
	else if (!strcmp(str, "normal")) m = SkBlendMode::kSrcOver;
	else if (!strcmp(str, "destination-over")) m = SkBlendMode::kDstOver;
	else if (!strcmp(str, "source-in")) m = SkBlendMode::kSrcIn;
	else if (!strcmp(str, "destination-in")) m = SkBlendMode::kDstIn;
	else if (!strcmp(str, "source-out")) m = SkBlendMode::kSrcOut;
	else if (!strcmp(str, "destination-out")) m = SkBlendMode::kDstOut;
	else if (!strcmp(str, "source-atop")) m = SkBlendMode::kSrcATop;
	else if (!strcmp(str, "destination-atop")) m = SkBlendMode::kDstATop;
	else if (!strcmp(str, "xor")) m = SkBlendMode::kXor;
	else if (!strcmp(str, "lighter")) m = SkBlendMode::kPlus;
	else if (!strcmp(str, "multiply")) m = SkBlendMode::kMultiply;
	else if (!strcmp(str, "screen")) m = SkBlendMode::kScreen;
	else if (!strcmp(str, "overlay")) m = SkBlendMode::kOverlay;
	else if (!strcmp(str, "darken")) m = SkBlendMode::kDarken;
	else if (!strcmp(str, "lighten")) m = SkBlendMode::kLighten;
	else if (!strcmp(str, "color-dodge")) m = SkBlendMode::kColorDodge;
	else if (!strcmp(str, "color-burn")) m = SkBlendMode::kColorBurn;
	else if (!strcmp(str, "hard-light")) m = SkBlendMode::kHardLight;
	else if (!strcmp(str, "soft-light")) m = SkBlendMode::kSoftLight;
	else if (!strcmp(str, "difference")) m = SkBlendMode::kDifference;
	else if (!strcmp(str, "exclusion")) m = SkBlendMode::kExclusion;
	else if (!strcmp(str, "hue")) m = SkBlendMode::kHue;
	else if (!strcmp(str, "saturation")) m = SkBlendMode::kSaturation;
	else if (!strcmp(str, "color")) m = SkBlendMode::kColor;
	else if (!strcmp(str, "luminosity")) m = SkBlendMode::kLuminosity;
	else ok = false;
	if (ok)
		context->state->blend_mode = m;
}

// ===========================================================================
// Slice 5: text (HarfBuzz shaping -> Skia glyphs)
// ===========================================================================

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
	std::vector<SkGlyphID> glyphs;
	std::vector<SkPoint> pos;
	layout_glyphs(context, *text, args[0], args[1], glyphs, pos);
	if (!glyphs.empty()) {
		SkFont font = current_font(context, font_size * scale);
		SkPaint p = make_fill_paint(context);
		cr->drawGlyphs(SkSpan<const SkGlyphID>(glyphs.data(), glyphs.size()),
		               SkSpan<const SkPoint>(pos.data(), pos.size()),
		               SkPoint::Make(0, 0), font, p);
	}
	if (scale != 1.)
		set_font_size(context, font_size);
}

void nx_canvas_context_2d_stroke_text(
    const FunctionCallbackInfo<Value> &info) {
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
	std::vector<SkGlyphID> glyphs;
	std::vector<SkPoint> pos;
	layout_glyphs(context, *text, args[0], args[1], glyphs, pos);
	if (!glyphs.empty()) {
		SkFont font = current_font(context, font_size * scale);
		// Accumulate each glyph's outline (offset to its position) into a path,
		// then stroke it.
		SkPathBuilder acc;
		for (size_t i = 0; i < glyphs.size(); i++) {
			std::optional<SkPath> gp = font.getPath(glyphs[i]);
			if (!gp)
				continue;
			SkMatrix m = SkMatrix::Translate(pos[i].x(), pos[i].y());
			acc.addPath(*gp, m, SkPath::kAppend_AddPathMode);
		}
		SkPaint p = make_stroke_paint(context);
		cr->drawPath(acc.snapshot(), p);
	}
	if (scale != 1.)
		set_font_size(context, font_size);
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

// save / restore — push/pop a full copy of the state node. SkCanvas::save
// covers matrix + clip; everything else lives on our state stack.
void nx_canvas_context_2d_save(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	cr->save();
	nx_canvas_context_2d_state_t *state =
	    new nx_canvas_context_2d_state_t(*context->state);  // copy ctor
	// Deep-copy the heap-owned font string; sk_sp/std::vector copy via ctor.
	if (context->state->font_string)
		state->font_string = strdup(context->state->font_string);
	state->next = context->state;
	context->state = state;
}

void nx_canvas_context_2d_restore(const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	if (context->state->next) {
		cr->restore();
		nx_canvas_context_2d_state_t *prev = context->state;
		context->state = prev->next;
		free_state_node(prev);
		set_font_size(context, context->state->font_size);
	}
}

// ===========================================================================
// Lifecycle: canvas / context creation + class registration
// ===========================================================================

void free_canvas(nx_canvas_t *c) {
	c->surface.reset();
	if (c->data)
		free(c->data);
	delete c;
}

void free_context_2d(nx_canvas_context_2d_t *context) {
	free_context_state(context->state);
	delete context;  // runs SkPath destructor
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
	nx_canvas_t *canvas = new nx_canvas_t();
	canvas->width = width;
	canvas->height = height;
	canvas->data = buffer;
	canvas->surface_dirty = false;
	canvas->surface = make_raster_surface(buffer, width, height);
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
	nx_canvas_context_2d_t *context = new nx_canvas_context_2d_t();
	context->canvas = canvas;
	context->ctx = canvas->surface ? canvas->surface->getCanvas() : nullptr;
	context->default_font_face = nullptr;
	nx_canvas_context_2d_state_t *state = new nx_canvas_context_2d_state_t();
	init_state_defaults(state);
	context->state = state;
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

void nx_canvas_gradient_init_class(const FunctionCallbackInfo<Value> &info) {
	(void)info;
}

// ===========================================================================
// Slice 6: isPointIn*, ImageData, drawImage, gradients, encode
// ===========================================================================

// ---- isPointInPath / isPointInStroke ----
static bool point_in_common(const FunctionCallbackInfo<Value> &info,
                            nx_canvas_context_2d_t *context, SkCanvas *cr,
                            Isolate *iso, bool stroke) {
	int base = 0;
	Local<Value> path;
	bool have_path = false;
	if (info.Length() > 0 && info[0]->IsObject()) {
		path = info[0];
		have_path = true;
		base = 1;
	}
	if (info.Length() - base < 2)
		return false;
	double args[2];
	if (!get_doubles(info, args, 2, base))
		return false;
	if (!stroke && info.Length() - base == 3 && info[base + 2]->IsString())
		set_fill_rule_v(iso, info[base + 2], context);
	SkPathBuilder saved = context->path;
	if (have_path) {
		context->path.reset();
		apply_path(iso, info.This(), path);
	}
	// context->path is in DEVICE space (CTM baked in at build time). Per spec
	// the query point is given in the SAME (already-transformed) space — it is
	// NOT re-transformed by the current CTM — so test it raw against the path.
	SkMatrix m = cr->getTotalMatrix();
	SkPath shape = context->path.snapshot();
	SkPoint pt = SkPoint::Make((SkScalar)args[0], (SkScalar)args[1]);
	bool is_in;
	if (stroke) {
		// The stroke pen is applied in user space and scaled by the CTM. Map
		// the device path back to user space, stroke it with the user pen, then
		// map the filled outline forward to device space and test the raw
		// query point against it.
		SkMatrix inv;
		SkPaint sp = make_stroke_paint(context);
		sp.setImageFilter(nullptr);
		if (m.invert(&inv)) {
			SkPath user = shape.makeTransform(inv);
			SkPath filled = skpathutils::FillPathWithPaint(user, sp);
			is_in = filled.makeTransform(m).contains(pt.x(), pt.y());
		} else {
			SkPath filled = skpathutils::FillPathWithPaint(shape, sp);
			is_in = filled.contains(pt.x(), pt.y());
		}
	} else {
		shape.setFillType(context->state->fill_rule);
		is_in = shape.contains(pt.x(), pt.y());
	}
	if (have_path)
		context->path = saved;
	return is_in;
}

void nx_canvas_context_2d_is_point_in_path(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	info.GetReturnValue().Set(
	    Boolean::New(iso, point_in_common(info, context, cr, iso, false)));
}
void nx_canvas_context_2d_is_point_in_stroke(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	info.GetReturnValue().Set(
	    Boolean::New(iso, point_in_common(info, context, cr, iso, true)));
}

// ---- putImageData (unpremultiplied RGBA -> premultiplied BGRA blit) ----
void nx_canvas_context_2d_put_image_data(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_THIS;
	Local<Context> jsctx = iso->GetCurrentContext();
	int sx = 0, sy = 0, sw = 0, sh = 0, dx, dy, image_data_width,
	    image_data_height, rows, cols;
	if (!info[0]->IsObject()) {
		nx_throw(iso, "putImageData: first argument must be an ImageData");
		return;
	}
	Local<Object> id = info[0].As<Object>();
	Local<Value> data_v, w_v, h_v;
	if (!id->Get(jsctx, nx_str(iso, "data")).ToLocal(&data_v) ||
	    !id->Get(jsctx, nx_str(iso, "width")).ToLocal(&w_v) ||
	    !id->Get(jsctx, nx_str(iso, "height")).ToLocal(&h_v))
		return;
	if (!data_v->IsArrayBufferView()) {
		nx_throw(iso, "ImageData.data must be a typed array");
		return;
	}
	Local<ArrayBufferView> view = data_v.As<ArrayBufferView>();
	uint8_t *src = (uint8_t *)view->Buffer()->Data() + view->ByteOffset();
	if (!w_v->Int32Value(jsctx).To(&image_data_width) ||
	    !h_v->Int32Value(jsctx).To(&image_data_height) ||
	    !info[1]->Int32Value(jsctx).To(&dx) ||
	    !info[2]->Int32Value(jsctx).To(&dy))
		return;
	bool gpu = context->canvas->gpu;
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

	// Destination: for a raster canvas, blit straight into canvas->data. For a
	// GPU canvas, premultiply+swizzle into a tight cols*rows scratch buffer and
	// upload it to the sub-rect via SkSurface::writePixels.
	int dstStride = gpu ? cols * 4 : (int)context->canvas->width * 4;
	uint8_t *dstBase;
	uint8_t *scratch = nullptr;
	if (gpu) {
		scratch = (uint8_t *)malloc((size_t)dstStride * rows);
		if (!scratch)
			return;
		dstBase = scratch;
	} else {
		dstBase = context->canvas->data + dstStride * dy + 4 * dx;
	}
	uint8_t *dst = dstBase;
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
	if (gpu) {
		SkImageInfo ii = SkImageInfo::Make(cols, rows, kBGRA_8888_SkColorType,
		                                   kPremul_SkAlphaType);
		SkPixmap pm(ii, scratch, (size_t)dstStride);
		context->canvas->surface->writePixels(pm, dx, dy);
		free(scratch);
	}
}

// ---- getImageData (premultiplied BGRA -> unpremultiplied RGBA) ----
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
	bool src_owned = false;
	uint8_t *src = canvas_readable_pixels(context->canvas, &src_owned);
	if (!src)
		return;
	Local<ArrayBuffer> ab = ArrayBuffer::New(iso, size);
	uint8_t *dst = (uint8_t *)ab->Data();
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
	if (src_owned)
		free(src);
	info.GetReturnValue().Set(ab);
}

// ---- drawImage ----
static SkSamplingOptions sampling_for(nx_canvas_context_2d_t *context) {
	if (!context->state->image_smoothing_enabled)
		return SkSamplingOptions(SkFilterMode::kNearest);
	switch (context->state->image_smoothing_quality) {
	case IMAGE_SMOOTHING_HIGH:
		return SkSamplingOptions(SkCubicResampler::Mitchell());
	case IMAGE_SMOOTHING_MEDIUM:
		return SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kLinear);
	default:
		return SkSamplingOptions(SkFilterMode::kLinear);
	}
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
	SkCanvas *cr = _c.cr;

	// Resolve the source as an SkImage (from a decoded image or another canvas).
	sk_sp<SkImage> image;
	double source_w = 0, source_h = 0;
	nx_image_t *img = nx_get_image(iso, info[0]);
	if (img) {
		if (!img->data || img->width == 0 || img->height == 0)
			return;
		// Memoize the wrapped SkImage on the image object. Rebuilding it per
		// draw gave each call a fresh image identity, defeating Ganesh's
		// GPU-texture cache (full source re-upload every frame), and at ~10k
		// tile draws/frame the per-call copy also OOMs the single-threaded GC.
		//
		// We build the cached image with RasterFromPixmapCopy, which COPIES
		// the pixels into a self-owned, immutable SkImage. An earlier version
		// used RasterFromData(MakeWithoutCopy) to avoid the copy, but that
		// shares `img->data` with a non-owning SkData; under the GPU backend
		// (especially with bilinear sampling) Skia's refcounting on the shared
		// buffer aborted (SkRefCnt BRK). The copy happens ONCE (it's cached),
		// so the cost is paid a single time per image, not per draw. Released
		// in close_image via nx_image_release_cache.
		if (!img->cached_sk_image) {
			SkImageInfo ii = SkImageInfo::Make(img->width, img->height,
			                                   kBGRA_8888_SkColorType,
			                                   kPremul_SkAlphaType);
			SkPixmap pm(ii, img->data, (size_t)img->width * 4);
			sk_sp<SkImage> cached = SkImages::RasterFromPixmapCopy(pm);
			img->cached_sk_image = new sk_sp<SkImage>(std::move(cached));
		}
		image = *static_cast<sk_sp<SkImage> *>(img->cached_sk_image);
		source_w = img->width;
		source_h = img->height;
	} else {
		nx_canvas_t *canvas = nx_get_canvas(iso, info[0]);
		if (!canvas || !canvas->surface) {
			nx_throw(iso, "Image or Canvas expected");
			return;
		}
		image = canvas->surface->makeImageSnapshot();
		source_w = canvas->width;
		source_h = canvas->height;
	}
	if (!image)
		return;

	double sx = 0, sy = 0, sw = source_w, sh = source_h, dx = 0, dy = 0,
	       dw = 0, dh = 0;
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
	SkRect srcR = SkRect::MakeXYWH((SkScalar)sx, (SkScalar)sy, (SkScalar)sw,
	                               (SkScalar)sh);
	SkRect dstR = SkRect::MakeXYWH((SkScalar)dx, (SkScalar)dy, (SkScalar)dw,
	                               (SkScalar)dh);
	SkPaint p;
	p.setAntiAlias(true);
	p.setBlendMode(context->state->blend_mode);
	p.setAlphaf((float)context->state->global_alpha);
	p.setImageFilter(shadow_filter(context));
	cr->drawImageRect(image, srcR, dstR, sampling_for(context), &p,
	                  SkCanvas::kStrict_SrcRectConstraint);
}

// ---- gradients ----
// Build (lazily) the SkShader from the gradient's buffered stops.
static void build_gradient_shader(nx_canvas_gradient_t *g) {
	size_t n = g->stops.size() / 5;
	if (n == 0) {
		g->shader = nullptr;
		return;
	}
	std::vector<SkColor4f> colors(n);
	std::vector<float> positions(n);
	for (size_t i = 0; i < n; i++) {
		positions[i] = (float)g->stops[i * 5 + 0];
		colors[i] = {(float)g->stops[i * 5 + 1], (float)g->stops[i * 5 + 2],
		             (float)g->stops[i * 5 + 3], (float)g->stops[i * 5 + 4]};
	}
	SkGradient::Colors gcolors(
	    SkSpan<const SkColor4f>(colors.data(), colors.size()),
	    SkSpan<const float>(positions.data(), positions.size()),
	    SkTileMode::kClamp);
	SkGradient grad(gcolors, SkGradient::Interpolation{});
	if (g->radial) {
		g->shader = SkShaders::TwoPointConicalGradient(
		    SkPoint::Make((SkScalar)g->x0, (SkScalar)g->y0), (float)g->r0,
		    SkPoint::Make((SkScalar)g->x1, (SkScalar)g->y1), (float)g->r1, grad);
	} else {
		SkPoint pts[2] = {SkPoint::Make((SkScalar)g->x0, (SkScalar)g->y0),
		                  SkPoint::Make((SkScalar)g->x1, (SkScalar)g->y1)};
		g->shader = SkShaders::LinearGradient(pts, grad);
	}
}

void nx_canvas_gradient_new_linear(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	double a[4];
	if (!get_doubles(info, a, 4, 0))
		return;
	nx_canvas_gradient_t *g = new nx_canvas_gradient_t();
	g->radial = false;
	g->x0 = a[0]; g->y0 = a[1]; g->x1 = a[2]; g->y1 = a[3];
	g->r0 = g->r1 = 0;
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_canvas_gradient_t>(iso, obj, g, free_gradient);
	info.GetReturnValue().Set(obj);
}
void nx_canvas_gradient_new_radial(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	double a[6];
	if (!get_doubles(info, a, 6, 0))
		return;
	nx_canvas_gradient_t *g = new nx_canvas_gradient_t();
	g->radial = true;
	g->x0 = a[0]; g->y0 = a[1]; g->r0 = a[2];
	g->x1 = a[3]; g->y1 = a[4]; g->r1 = a[5];
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_canvas_gradient_t>(iso, obj, g, free_gradient);
	info.GetReturnValue().Set(obj);
}
void nx_canvas_gradient_add_color_stop(
    const FunctionCallbackInfo<Value> &info) {
	nx_canvas_gradient_t *g = nx::Unwrap<nx_canvas_gradient_t>(info[0]);
	if (!g)
		return;
	double a[5];  // offset, r, g, b (0..255), a (0..1)
	if (!get_doubles(info, a, 5, 1))
		return;
	g->stops.push_back(a[0]);
	g->stops.push_back(a[1] / 255.);
	g->stops.push_back(a[2] / 255.);
	g->stops.push_back(a[3] / 255.);
	g->stops.push_back(a[4]);
	g->shader = nullptr;  // invalidate; rebuilt on next use
}
void nx_canvas_context_2d_set_fill_style_gradient(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_ARGV0;
	nx_canvas_gradient_t *g = nx::Unwrap<nx_canvas_gradient_t>(info[1]);
	if (!g)
		return;
	build_gradient_shader(g);
	context->state->fill_gradient = g;
	context->state->fill_source_type = SOURCE_GRADIENT;
}
void nx_canvas_context_2d_set_stroke_style_gradient(
    const FunctionCallbackInfo<Value> &info) {
	ENTER_ARGV0;
	nx_canvas_gradient_t *g = nx::Unwrap<nx_canvas_gradient_t>(info[1]);
	if (!g)
		return;
	build_gradient_shader(g);
	context->state->stroke_gradient = g;
	context->state->stroke_source_type = SOURCE_GRADIENT;
}

// ---- encode (PNG via Skia; JPEG/WebP via turbojpeg/libwebp over raw BGRA) ----
static uint8_t *bgra_to_rgb(const uint8_t *bgra, int width, int height,
                            int stride) {
	uint8_t *rgb = (uint8_t *)malloc((size_t)width * height * 3);
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
// Encode raw premultiplied BGRA (w,h,stride) to PNG/JPEG/WebP. type: 0=png,
// 1=jpeg, 2=webp.
static int encode_pixels(const uint8_t *data, int width, int height,
                         int stride, int type, double quality, uint8_t **out,
                         size_t *out_size) {
	if (type == 1) {
		uint8_t *rgb = bgra_to_rgb(data, width, height, stride);
		if (!rgb)
			return -1;
		tjhandle h = tjInitCompress();
		if (!h) { free(rgb); return -1; }
		unsigned char *jpeg = NULL;
		unsigned long jsize = 0;
		int r = tjCompress2(h, rgb, width, 0, height, TJPF_RGB, &jpeg, &jsize,
		                    TJSAMP_420, (int)(quality * 100), TJFLAG_FASTDCT);
		free(rgb);
		tjDestroy(h);
		if (r != 0) { if (jpeg) tjFree(jpeg); return -1; }
		*out = (uint8_t *)malloc(jsize);
		if (!*out) { tjFree(jpeg); return -1; }
		memcpy(*out, jpeg, jsize);
		*out_size = jsize;
		tjFree(jpeg);
		return 0;
	}
	if (type == 2) {
		uint8_t *webp = NULL;
		size_t wsize = WebPEncodeBGRA(data, width, height, stride,
		                              (float)(quality * 100), &webp);
		if (wsize == 0 || !webp)
			return -1;
		*out = (uint8_t *)malloc(wsize);
		if (!*out) { WebPFree(webp); return -1; }
		memcpy(*out, webp, wsize);
		*out_size = wsize;
		WebPFree(webp);
		return 0;
	}
	// PNG via Skia.
	SkImageInfo ii = SkImageInfo::Make(width, height, kBGRA_8888_SkColorType,
	                                   kPremul_SkAlphaType);
	SkPixmap pm(ii, data, stride);
	SkDynamicMemoryWStream stream;
	SkPngEncoder::Options opts;
	if (!SkPngEncoder::Encode(&stream, pm, opts))
		return -1;
	size_t sz = stream.bytesWritten();
	*out = (uint8_t *)malloc(sz);
	if (!*out)
		return -1;
	stream.copyTo(*out);
	*out_size = sz;
	return 0;
}

static int mime_to_type_code(const char *type) {
	if (type) {
		if (!strcmp(type, "image/jpeg")) return 1;
		if (!strcmp(type, "image/webp")) return 2;
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

// Async encode: snapshot the canvas pixels (BGRA copy) on the main thread, then
// encode on the thread pool.
typedef struct {
	uint8_t *pixels;  // owned BGRA copy
	int width, height, stride;
	int type;
	double quality;
	uint8_t *result;
	size_t result_size;
	int err;
} encode_async_t;

static uint8_t *snapshot_pixels(nx_canvas_t *canvas, int *w, int *h,
                                int *stride) {
	int cw = canvas->width ? (int)canvas->width : 1;
	int ch = canvas->height ? (int)canvas->height : 1;
	int st = cw * 4;
	uint8_t *copy = (uint8_t *)calloc(1, (size_t)st * ch);
	if (copy && canvas->width && canvas->height) {
		bool owned = false;
		uint8_t *src = canvas_readable_pixels(canvas, &owned);
		if (src) {
			memcpy(copy, src, (size_t)st * ch);
			if (owned)
				free(src);
		}
	}
	*w = cw;
	*h = ch;
	*stride = st;
	return copy;
}

void nx_canvas_encode_do(nx_work_t *req) {
	encode_async_t *data = (encode_async_t *)req->data;
	data->err = encode_pixels(data->pixels, data->width, data->height,
	                          data->stride, data->type, data->quality,
	                          &data->result, &data->result_size);
}
MaybeLocal<Value> nx_canvas_encode_cb(Isolate *iso, nx_work_t *req) {
	encode_async_t *data = (encode_async_t *)req->data;
	free(data->pixels);
	data->pixels = nullptr;
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
		if (!info[2]->NumberValue(iso->GetCurrentContext()).To(&quality))
			quality = 0.92;
	if (quality < 0.0) quality = 0.0;
	if (quality > 1.0) quality = 1.0;
	NX_INIT_WORK_T(encode_async_t);
	data->pixels =
	    snapshot_pixels(canvas, &data->width, &data->height, &data->stride);
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
		if (!info[1]->NumberValue(iso->GetCurrentContext()).To(&quality))
			quality = 0.92;
	if (quality < 0.0) quality = 0.0;
	if (quality > 1.0) quality = 1.0;
	int w, h, stride;
	uint8_t *pixels = snapshot_pixels(canvas, &w, &h, &stride);
	uint8_t *buf = NULL;
	size_t buf_size = 0;
	int rc = encode_pixels(pixels, w, h, stride, type_code, quality, &buf,
	                       &buf_size);
	free(pixels);
	if (rc != 0) {
		nx_throw(iso, "Failed to encode data URL");
		return;
	}
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

// Release an nx_image_t's memoized SkImage. Declared in image.h, defined here
// where the Skia type is visible; called from image.cc:close_image and
// irs.cc (in-place pixel updates must drop the stale memo).
void nx_image_release_cache(nx_image_t *image) {
	if (image && image->cached_sk_image) {
		delete static_cast<sk_sp<SkImage> *>(image->cached_sk_image);
		image->cached_sk_image = nullptr;
	}
}

bool nx_resolve_round_rect_radii(Isolate *iso, Local<Value> radii,
                                 SkVector out[4]) {
	return resolve_round_rect_radii_impl(iso, radii, out);
}

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

void nx_canvas_set_gpu_surface(nx_canvas_t *c, sk_sp<SkSurface> surface) {
	if (!c)
		return;
	// Release the raster backing; adopt the GPU surface.
	c->surface.reset();
	if (c->data) {
		free(c->data);
		c->data = nullptr;
	}
	c->surface = std::move(surface);
	c->gpu = true;
	c->surface_dirty = true;  // ensure_surface() will (re)wire context->ctx
}

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
