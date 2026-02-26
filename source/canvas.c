/**
 * Mostly based off of `node-canvas` - MIT license
 * https://github.com/Automattic/node-canvas
 */

#include "canvas.h"
#include "dommatrix.h"
#include "font.h"
#include "image.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * Lazily recreates the canvas surface and resets the 2D context state after
 * the canvas has been resized. Per the HTML spec, setting width or height
 * must reset the rendering context to its default state.
 */
static void nx_canvas_ensure_surface(JSContext *ctx,
								 nx_canvas_context_2d_t *context) {
	nx_canvas_t *canvas = context->canvas;
	if (canvas->surface)
		return;

	// Handle zero-width/height dimensions: skip surface creation.
	// Drawing on a 0-dimension canvas is a no-op per spec, and
	// lazy init handles this naturally.
	if (canvas->width == 0 || canvas->height == 0) {
		// Still need to reset context state below, but no surface/buffer
		goto reset_state;
	}

	// Overflow check for buffer size calculation (width * height * 4).
	// Also covers stride overflow (width * 4).
	if (canvas->width > SIZE_MAX / 4 ||
		(size_t)canvas->height > SIZE_MAX / ((size_t)canvas->width * 4)) {
		JS_ThrowRangeError(ctx, "Canvas dimensions too large");
		return;
	}

	// Allocate new backing buffer (zeroed = transparent black)
	size_t buf_size = (size_t)canvas->width * canvas->height * 4;
	uint8_t *buffer = js_mallocz(ctx, buf_size);
	if (!buffer) {
		JS_ThrowOutOfMemory(ctx);
		return;
	}
	canvas->data = buffer;
	canvas->surface = cairo_image_surface_create_for_data(
		buffer, CAIRO_FORMAT_ARGB32, canvas->width, canvas->height,
		canvas->width * 4);

	// Destroy old cairo context
	if (context->ctx) {
		cairo_destroy(context->ctx);
	}
	context->ctx = cairo_create(canvas->surface);

reset_state:
	// Clear the current path
	if (context->path) {
		cairo_path_destroy(context->path);
		context->path = NULL;
	}

	// Free the drawing state stack (keep only the top-level state)
	nx_canvas_context_2d_state_t *state = context->state;
	if (state->next) {
		// Can't use the recursive finalizer since it uses `js_free_rt`,
		// but we have a JSContext here. Free the chain manually.
		nx_canvas_context_2d_state_t *s = state->next;
		while (s) {
			nx_canvas_context_2d_state_t *next = s->next;
			if (s->font_string)
				free((void *)s->font_string);
			if (s->fill_gradient)
				cairo_pattern_destroy(s->fill_gradient);
			if (s->stroke_gradient)
				cairo_pattern_destroy(s->stroke_gradient);
			js_free(ctx, s);
			s = next;
		}
		state->next = NULL;
	}

	// Reset the drawing state to defaults (per HTML canvas spec)
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
	state->font = JS_UNDEFINED;
	state->font_size = 10.;
	state->ft_face = NULL;
	state->hb_font = NULL;
	state->fill = (nx_rgba_t){0., 0., 0., 1.};     // black
	state->stroke = (nx_rgba_t){0., 0., 0., 1.};   // black
	state->fill_source_type = SOURCE_RGBA;
	state->stroke_source_type = SOURCE_RGBA;
	state->global_alpha = 1.;
	state->image_smoothing_quality = CAIRO_FILTER_FAST;
	state->image_smoothing_enabled = true;
	state->text_align = TEXT_ALIGN_START;
	state->text_baseline = TEXT_BASELINE_ALPHABETIC;

	// Reset cairo state (only if we have a context)
	if (context->ctx) {
		cairo_set_line_width(context->ctx, 1.);
	}
}

#define CANVAS_CONTEXT_ARGV0                                                   \
	nx_canvas_context_2d_t *context =                                          \
		JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);               \
	if (!context) {                                                            \
		return JS_EXCEPTION;                                                   \
	}                                                                          \
	nx_canvas_ensure_surface(ctx, context);                                    \
	cairo_t *cr = context->ctx;                                                \
	(void)cr;

#define CANVAS_CONTEXT_THIS                                                    \
	nx_canvas_context_2d_t *context =                                          \
		JS_GetOpaque2(ctx, this_val, nx_canvas_context_class_id);              \
	if (!context) {                                                            \
		return JS_EXCEPTION;                                                   \
	}                                                                          \
	nx_canvas_ensure_surface(ctx, context);                                    \
	cairo_t *cr = context->ctx;                                                \
	(void)cr;

#define RECT_ARGS                                                              \
	double args[4];                                                            \
	if (js_validate_doubles_args(ctx, argv, args, 4, 0)) {                     \
		return JS_EXCEPTION;                                                   \
	}                                                                          \
	double x = args[0];                                                        \
	double y = args[1];                                                        \
	double width = args[2];                                                    \
	double height = args[3];

typedef struct Point {
	float x;
	float y;
} Point;

static JSClassID nx_canvas_class_id;
static JSClassID nx_canvas_context_class_id;
static JSClassID nx_canvas_gradient_class_id;

static inline int min(int a, int b) { return a < b ? a : b; }

static inline float minf(float a, float b) { return a < b ? a : b; }

static inline void generic_swap(void *a, void *b, size_t size) {
	// Temporary storage for the swap
	char temp[size];
	memcpy(temp, a, size);
	memcpy(a, b, size);
	memcpy(b, temp, size);
}

static int js_validate_doubles_args(JSContext *ctx, JSValueConst *argv,
									double *args, size_t count, size_t offset) {
	int result = 0;
	for (size_t i = 0; i < count; i++) {
		if (JS_ToFloat64(ctx, &args[i], argv[offset + i])) {
			result = 1;
			break;
		}
	}
	return result;
}

static void set_fill_rule(JSContext *ctx, JSValueConst fill_rule, cairo_t *cr) {
	cairo_fill_rule_t rule = CAIRO_FILL_RULE_WINDING;
	if (JS_IsString(fill_rule)) {
		const char *str = JS_ToCString(ctx, fill_rule);
		if (!str)
			return;
		if (strcmp(str, "evenodd") == 0) {
			rule = CAIRO_FILL_RULE_EVEN_ODD;
		}
		JS_FreeCString(ctx, str);
	}
	cairo_set_fill_rule(cr, rule);
}

static void apply_path(JSContext *ctx, JSValue this_val, JSValue path) {
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	JSValue apply_path_func =
		JS_GetPropertyStr(ctx, nx_ctx->init_obj, "applyPath");
	JSValue apply_path_argv[] = {this_val, path};
	JS_Call(ctx, apply_path_func, JS_NULL, 2, apply_path_argv);
	JS_FreeValue(ctx, apply_path_func);
}

static void save_path(nx_canvas_context_2d_t *context) {
	context->path = cairo_copy_path_flat(context->ctx);
	cairo_new_path(context->ctx);
}

static void restore_path(nx_canvas_context_2d_t *context) {
	cairo_new_path(context->ctx);
	cairo_append_path(context->ctx, context->path);
	cairo_path_destroy(context->path);
}

static void fill(nx_canvas_context_2d_t *context, bool preserve) {
	if (context->state->fill_source_type == SOURCE_GRADIENT &&
		context->state->fill_gradient) {
		cairo_set_source(context->ctx, context->state->fill_gradient);
	} else {
		cairo_set_source_rgba(context->ctx, context->state->fill.r,
							  context->state->fill.g, context->state->fill.b,
							  context->state->fill.a *
								  context->state->global_alpha);
	}
	if (preserve) {
		cairo_fill_preserve(context->ctx);
	} else {
		cairo_fill(context->ctx);
	}
}

static void stroke(nx_canvas_context_2d_t *context, bool preserve) {
	if (context->state->stroke_source_type == SOURCE_GRADIENT &&
		context->state->stroke_gradient) {
		cairo_set_source(context->ctx, context->state->stroke_gradient);
	} else {
		cairo_set_source_rgba(context->ctx, context->state->stroke.r,
							  context->state->stroke.g, context->state->stroke.b,
							  context->state->stroke.a *
								  context->state->global_alpha);
	}
	if (preserve) {
		cairo_stroke_preserve(context->ctx);
	} else {
		cairo_stroke(context->ctx);
	}
}

static void set_font_size(nx_canvas_context_2d_t *context, double font_size) {
	FT_Set_Char_Size(context->state->ft_face, 0, font_size * 64.0, 0, 0);
	cairo_set_font_size(context->ctx, font_size);
	hb_font_set_scale(context->state->hb_font, font_size * 64, font_size * 64);
}

static JSValue nx_canvas_context_2d_move_to(JSContext *ctx,
											JSValueConst this_val, int argc,
											JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	double args[2];
	if (js_validate_doubles_args(ctx, argv, args, 2, 0))
		return JS_EXCEPTION;
	cairo_move_to(cr, args[0], args[1]);
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_is_point_in_path(JSContext *ctx,
													 JSValueConst this_val,
													 int argc,
													 JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	JSValue path = JS_NULL;
	if (argc > 0 && JS_IsObject(argv[0])) {
		path = argv[0];
		argc--;
		argv++;
	}
	bool is_in = false;
	if (argc >= 2) {
		double args[2];
		if (js_validate_doubles_args(ctx, argv, args, 2, 0))
			return JS_EXCEPTION;

		if (argc == 3 && JS_IsString(argv[2])) {
			set_fill_rule(ctx, argv[2], cr);
		}
		bool needs_restore = false;
		if (!JS_IsNull(path)) {
			needs_restore = true;
			save_path(context);
			apply_path(ctx, this_val, path);
		}

		nx_dommatrix_t matrix = {0};
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

		if (needs_restore) {
			restore_path(context);
		}
	}
	return JS_NewBool(ctx, is_in);
}

static JSValue nx_canvas_context_2d_is_point_in_stroke(JSContext *ctx,
													   JSValueConst this_val,
													   int argc,
													   JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	JSValue path = JS_NULL;
	if (argc > 0 && JS_IsObject(argv[0])) {
		path = argv[0];
		argc--;
		argv++;
	}
	bool is_in = false;
	if (argc >= 2) {
		double args[2];
		if (js_validate_doubles_args(ctx, argv, args, 2, 0))
			return JS_EXCEPTION;
		bool needs_restore = false;
		if (!JS_IsNull(path)) {
			needs_restore = true;
			save_path(context);
			apply_path(ctx, this_val, path);
		}

		nx_dommatrix_t matrix = {0};
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

		if (needs_restore) {
			restore_path(context);
		}
	}
	return JS_NewBool(ctx, is_in);
}

static JSValue nx_canvas_context_2d_line_to(JSContext *ctx,
											JSValueConst this_val, int argc,
											JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	double args[2];
	if (js_validate_doubles_args(ctx, argv, args, 2, 0))
		return JS_EXCEPTION;
	cairo_line_to(cr, args[0], args[1]);
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_bezier_curve_to(JSContext *ctx,
													JSValueConst this_val,
													int argc,
													JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	double args[6];
	if (js_validate_doubles_args(ctx, argv, args, 6, 0))
		return JS_EXCEPTION;
	cairo_curve_to(cr, args[0], args[1], args[2], args[3], args[4], args[5]);
	return JS_UNDEFINED;
}

/*
 * Quadratic curve approximation from libsvg-cairo.
 */
static JSValue nx_canvas_context_2d_quadratic_curve_to(JSContext *ctx,
													   JSValueConst this_val,
													   int argc,
													   JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	double args[4];
	if (js_validate_doubles_args(ctx, argv, args, 4, 0))
		return JS_EXCEPTION;
	double x, y, x1 = args[0], y1 = args[1], x2 = args[2], y2 = args[3];

	cairo_get_current_point(cr, &x, &y);

	if (0 == x && 0 == y) {
		x = x1;
		y = y1;
	}

	cairo_curve_to(cr, x + 2.0 / 3.0 * (x1 - x), y + 2.0 / 3.0 * (y1 - y),
				   x2 + 2.0 / 3.0 * (x1 - x2), y2 + 2.0 / 3.0 * (y1 - y2), x2,
				   y2);
	return JS_UNDEFINED;
}

static double twoPi = M_PI * 2.;

// Adapted from
// https://chromium.googlesource.com/chromium/blink/+/refs/heads/main/Source/modules/canvas2d/CanvasPathMethods.cpp
static void canonicalizeAngle(double *startAngle, double *endAngle) {
	// Make 0 <= startAngle < 2*PI
	double newStartAngle = fmod(*startAngle, twoPi);
	if (newStartAngle < 0) {
		newStartAngle += twoPi;
		// Check for possible catastrophic cancellation in cases where
		// newStartAngle was a tiny negative number (c.f. crbug.com/503422)
		if (newStartAngle >= twoPi)
			newStartAngle -= twoPi;
	}
	double delta = newStartAngle - *startAngle;
	*startAngle = newStartAngle;
	*endAngle = *endAngle + delta;
}

// Adapted from
// https://chromium.googlesource.com/chromium/blink/+/refs/heads/main/Source/modules/canvas2d/CanvasPathMethods.cpp
static double adjustEndAngle(double startAngle, double endAngle,
							 int counterclockwise) {
	double newEndAngle = endAngle;
	/* http://www.whatwg.org/specs/web-apps/current-work/multipage/the-canvas-element.html#dom-context-2d-arc
	 * If the counterclockwise argument is false and endAngle-startAngle is
	 * equal to or greater than 2pi, or, if the counterclockwise argument is
	 * true and startAngle-endAngle is equal to or greater than 2pi, then the
	 * arc is the whole circumference of this ellipse, and the point at
	 * startAngle along this circle's circumference, measured in radians
	 * clockwise from the ellipse's semi-major axis, acts as both the start
	 * point and the end point.
	 */
	if (!counterclockwise && endAngle - startAngle >= twoPi)
		newEndAngle = startAngle + twoPi;
	else if (counterclockwise && startAngle - endAngle >= twoPi)
		newEndAngle = startAngle - twoPi;
	/*
	 * Otherwise, the arc is the path along the circumference of this ellipse
	 * from the start point to the end point, going anti-clockwise if the
	 * counterclockwise argument is true, and clockwise otherwise. Since the
	 * points are on the ellipse, as opposed to being simply angles from zero,
	 * the arc can never cover an angle greater than 2pi radians.
	 */
	/* NOTE: When startAngle = 0, endAngle = 2Pi and counterclockwise = true,
	 * the spec does not indicate clearly. We draw the entire circle, because
	 * some web sites use arc(x, y, r, 0, 2*Math.PI, true) to draw circle. We
	 * preserve backward-compatibility.
	 */
	else if (!counterclockwise && startAngle > endAngle)
		newEndAngle = startAngle + (twoPi - fmod(startAngle - endAngle, twoPi));
	else if (counterclockwise && startAngle < endAngle)
		newEndAngle = startAngle - (twoPi - fmod(endAngle - startAngle, twoPi));
	return newEndAngle;
}

/*
 * Adds an arc at x, y with the given radii and start/end angles.
 */
static JSValue nx_canvas_context_2d_arc(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	double args[5];
	if (js_validate_doubles_args(ctx, argv, args, 5, 0))
		return JS_EXCEPTION;

	double x = args[0];
	double y = args[1];
	double radius = args[2];
	double startAngle = args[3];
	double endAngle = args[4];

	if (radius < 0) {
		JS_ThrowRangeError(ctx, "The radius provided is negative.");
		return JS_EXCEPTION;
	}

	int counterclockwise = argc > 5 ? JS_ToBool(ctx, argv[5]) : 0;
	if (counterclockwise == -1)
		return JS_EXCEPTION;

	canonicalizeAngle(&startAngle, &endAngle);
	endAngle = adjustEndAngle(startAngle, endAngle, counterclockwise);

	if (counterclockwise) {
		cairo_arc_negative(cr, x, y, radius, startAngle, endAngle);
	} else {
		cairo_arc(cr, x, y, radius, startAngle, endAngle);
	}

	return JS_UNDEFINED;
}

/*
 * Adds an arcTo point (x0,y0) to (x1,y1) with the given radius.
 *
 * Implementation influenced by WebKit.
 */
static JSValue nx_canvas_context_2d_arc_to(JSContext *ctx,
										   JSValueConst this_val, int argc,
										   JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	double args[5];
	if (js_validate_doubles_args(ctx, argv, args, 5, 0))
		return JS_EXCEPTION;

	// Current path point
	double x, y;
	cairo_get_current_point(cr, &x, &y);
	Point p0 = {x, y};

	// Point (x0,y0)
	Point p1 = {args[0], args[1]};

	// Point (x1,y1)
	Point p2 = {args[2], args[3]};

	float radius = args[4];

	if ((p1.x == p0.x && p1.y == p0.y) || (p1.x == p2.x && p1.y == p2.y) ||
		radius == 0.f) {
		cairo_line_to(cr, p1.x, p1.y);
		return JS_UNDEFINED;
	}

	Point p1p0 = {(p0.x - p1.x), (p0.y - p1.y)};
	Point p1p2 = {(p2.x - p1.x), (p2.y - p1.y)};
	float p1p0_length = sqrtf(p1p0.x * p1p0.x + p1p0.y * p1p0.y);
	float p1p2_length = sqrtf(p1p2.x * p1p2.x + p1p2.y * p1p2.y);

	double cos_phi =
		(p1p0.x * p1p2.x + p1p0.y * p1p2.y) / (p1p0_length * p1p2_length);
	// all points on a line logic
	if (-1 == cos_phi) {
		cairo_line_to(cr, p1.x, p1.y);
		return JS_UNDEFINED;
	}

	if (1 == cos_phi) {
		// add infinite far away point
		unsigned int max_length = 65535;
		double factor_max = max_length / p1p0_length;
		Point ep = {(p0.x + factor_max * p1p0.x), (p0.y + factor_max * p1p0.y)};
		cairo_line_to(cr, ep.x, ep.y);
		return JS_UNDEFINED;
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

	if (anticlockwise && M_PI * 2 != radius) {
		cairo_arc_negative(cr, p.x, p.y, radius, sa, ea);
	} else {
		cairo_arc(cr, p.x, p.y, radius, sa, ea);
	}

	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_ellipse(JSContext *ctx,
											JSValueConst this_val, int argc,
											JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	double args[7];
	if (js_validate_doubles_args(ctx, argv, args, 7, 0))
		return JS_EXCEPTION;

	double radiusX = args[2];
	double radiusY = args[3];

	if (radiusX == 0 || radiusY == 0)
		return JS_UNDEFINED;

	double x = args[0];
	double y = args[1];
	double rotation = args[4];
	double startAngle = args[5];
	double endAngle = args[6];
	int anticlockwise = argc >= 8 ? JS_ToBool(ctx, argv[7]) : 0;
	if (anticlockwise == -1)
		return JS_EXCEPTION;

	// See https://www.cairographics.org/cookbook/ellipses/
	double xRatio = radiusX / radiusY;

	cairo_matrix_t save_matrix;
	cairo_get_matrix(cr, &save_matrix);
	cairo_translate(cr, x, y);
	cairo_rotate(cr, rotation);
	cairo_scale(cr, xRatio, 1.0);
	cairo_translate(cr, -x, -y);
	if (anticlockwise && M_PI * 2 != args[4]) {
		cairo_arc_negative(cr, x, y, radiusY, startAngle, endAngle);
	} else {
		cairo_arc(cr, x, y, radiusY, startAngle, endAngle);
	}
	cairo_set_matrix(cr, &save_matrix);
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_rect(JSContext *ctx, JSValueConst this_val,
										 int argc, JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	RECT_ARGS;
	if (width == 0) {
		cairo_move_to(cr, x, y);
		cairo_line_to(cr, x, y + height);
	} else if (height == 0) {
		cairo_move_to(cr, x, y);
		cairo_line_to(cr, x + width, y);
	} else {
		cairo_rectangle(cr, x, y, width, height);
	}
	return JS_UNDEFINED;
}

// Draws an arc with two potentially different radii.
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

inline static bool get_radius(JSContext *ctx, JSValue v, Point *p) {
	if (JS_IsObject(v)) { // 5.1 DOMPointInit
		JSValue rx = JS_GetPropertyStr(ctx, v, "x");
		JSValue ry = JS_GetPropertyStr(ctx, v, "y");
		if (!JS_IsNumber(rx) || !JS_IsNumber(ry)) {
			JS_FreeValue(ctx, rx);
			JS_FreeValue(ctx, ry);
			JS_ThrowTypeError(ctx, "A DOMPoint object must be provided");
			return true;
		}
		double rxv;
		double ryv;
		if (JS_ToFloat64(ctx, &rxv, rx) || JS_ToFloat64(ctx, &ryv, ry)) {
			JS_FreeValue(ctx, rx);
			JS_FreeValue(ctx, ry);
			return true;
		}
		JS_FreeValue(ctx, rx);
		JS_FreeValue(ctx, ry);
		if (!isfinite(rxv) || !isfinite(ryv))
			return true;
		if (rxv < 0 || ryv < 0) {
			JS_ThrowRangeError(ctx, "radii must be positive.");
			return true;
		}
		p->x = rxv;
		p->y = ryv;
		return false;
	} else if (JS_IsNumber(v)) { // 5.2 unrestricted double
		double rv;
		if (JS_ToFloat64(ctx, &rv, v)) {
			return true;
		}
		if (!isfinite(rv))
			return true;
		if (rv < 0) {
			JS_ThrowRangeError(ctx, "radii must be positive.");
			return true;
		}
		p->x = p->y = rv;
		return false;
	}
	JS_ThrowTypeError(ctx, "Unsupported radii value.");
	return true;
}

static JSValue nx_canvas_context_2d_round_rect(JSContext *ctx,
											   JSValueConst this_val, int argc,
											   JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	RECT_ARGS;

	// 4. Let normalizedRadii be an empty list
	Point normalizedRadii[4];
	u32 nRadii = 4;

	if (argc < 5 || JS_IsUndefined(argv[4])) {
		for (size_t i = 0; i < 4; i++)
			normalizedRadii[i].x = normalizedRadii[i].y = 0.;
	} else if (JS_IsArray(argv[4])) {
		JSValue length = JS_GetPropertyStr(ctx, argv[4], "length");
		if (JS_ToUint32(ctx, &nRadii, length)) {
			JS_FreeValue(ctx, length);
			return JS_EXCEPTION;
		}
		JS_FreeValue(ctx, length);
		if (!(nRadii >= 1 && nRadii <= 4)) {
			JS_ThrowRangeError(
				ctx, "radii must be a list of one, two, three or four radii.");
			return JS_EXCEPTION;
		}
		// 5. For each radius of radii
		for (size_t i = 0; i < nRadii; i++) {
			JSValue v = JS_GetPropertyUint32(ctx, argv[4], i);
			if (get_radius(ctx, v, &normalizedRadii[i])) {
				JS_FreeValue(ctx, v);
				return JS_EXCEPTION;
			}
			JS_FreeValue(ctx, v);
		}
	} else {
		// 2. If radii is a double, then set radii to <<radii>>
		if (get_radius(ctx, argv[4], &normalizedRadii[0])) {
			return JS_EXCEPTION;
		}
		for (size_t i = 1; i < 4; i++) {
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
		upperLeft = normalizedRadii[0];
		upperRight = normalizedRadii[0];
		lowerRight = normalizedRadii[0];
		lowerLeft = normalizedRadii[0];
	}

	bool clockwise = true;
	if (width < 0) {
		clockwise = false;
		x += width;
		width = -width;
		generic_swap(&upperLeft, &upperRight, sizeof(Point));
		generic_swap(&lowerLeft, &lowerRight, sizeof(Point));
	}

	if (height < 0) {
		clockwise = !clockwise;
		y += height;
		height = -height;
		generic_swap(&upperLeft, &upperRight, sizeof(Point));
		generic_swap(&lowerLeft, &lowerRight, sizeof(Point));
	}

	// 11. Corner curves must not overlap. Scale radii to prevent this.
	{
		float top = upperLeft.x + upperRight.x;
		float right = upperRight.y + lowerRight.y;
		float bottom = lowerRight.x + lowerLeft.x;
		float left = upperLeft.y + lowerLeft.y;
		float scale =
			minf(width / top,
				 minf(height / right, minf(width / bottom, height / left)));
		if (scale < 1.) {
			upperLeft.x *= scale;
			upperLeft.y *= scale;
			upperRight.x *= scale;
			upperRight.x *= scale;
			lowerLeft.y *= scale;
			lowerLeft.y *= scale;
			lowerRight.y *= scale;
			lowerRight.y *= scale;
		}
	}

	// 12. Draw
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
		elli_arc(cr, x + upperLeft.x, y + upperLeft.y, upperLeft.x, upperLeft.y,
				 M_PI, 3. * M_PI / 2., true);
	} else {
		elli_arc(cr, x + upperLeft.x, y + upperLeft.y, upperLeft.x, upperLeft.y,
				 M_PI, 3. * M_PI / 2., false);
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
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_get_font(JSContext *ctx,
											 JSValueConst this_val, int argc,
											 JSValueConst *argv) {
	CANVAS_CONTEXT_ARGV0;
	return JS_NewString(
		ctx, context->state->font_string ? context->state->font_string : "");
}

static JSValue nx_canvas_context_2d_set_font(JSContext *ctx,
											 JSValueConst this_val, int argc,
											 JSValueConst *argv) {
	CANVAS_CONTEXT_ARGV0;

	context->state->font = argv[1];
	nx_font_face_t *face = nx_get_font_face(ctx, context->state->font);
	if (!face)
		return JS_EXCEPTION;

	double font_size;
	if (JS_ToFloat64(ctx, &font_size, argv[2]))
		return JS_EXCEPTION;

	const char *font_string = JS_ToCString(ctx, argv[3]);
	if (!font_string)
		return JS_EXCEPTION;

	context->state->font_size = font_size;
	context->state->font_string = strdup(font_string);
	context->state->ft_face = face->ft_face;
	context->state->hb_font = face->hb_font;
	cairo_set_font_face(cr, face->cairo_font);
	set_font_size(context, font_size);
	JS_FreeCString(ctx, font_string);
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_get_transform(JSContext *ctx,
												  JSValueConst this_val,
												  int argc,
												  JSValueConst *argv) {
	CANVAS_CONTEXT_ARGV0;
	cairo_matrix_t matrix;
	cairo_get_matrix(cr, &matrix);
	JSValue array = JS_NewArray(ctx);
	JS_SetPropertyUint32(ctx, array, 0, JS_NewFloat64(ctx, matrix.xx));
	JS_SetPropertyUint32(ctx, array, 1, JS_NewFloat64(ctx, matrix.yx));
	JS_SetPropertyUint32(ctx, array, 2, JS_NewFloat64(ctx, matrix.xy));
	JS_SetPropertyUint32(ctx, array, 3, JS_NewFloat64(ctx, matrix.yy));
	JS_SetPropertyUint32(ctx, array, 4, JS_NewFloat64(ctx, matrix.x0));
	JS_SetPropertyUint32(ctx, array, 5, JS_NewFloat64(ctx, matrix.y0));
	return array;
}

static JSValue nx_canvas_context_2d_stroke_rect(JSContext *ctx,
												JSValueConst this_val, int argc,
												JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	RECT_ARGS;
	if (width && height) {
		save_path(context);
		cairo_rectangle(cr, x, y, width, height);
		stroke(context, false);
		restore_path(context);
	}
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_clear_rect(JSContext *ctx,
											   JSValueConst this_val, int argc,
											   JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	RECT_ARGS;
	if (width && height) {
		cairo_save(cr);
		save_path(context);
		cairo_rectangle(cr, x, y, width, height);
		cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
		cairo_fill(cr);
		restore_path(context);
		cairo_restore(cr);
	}
	return JS_UNDEFINED;
}

double get_text_scale(nx_canvas_context_2d_t *context, const char *text,
					  double max_width) {
	// Create HarfBuzz buffer
	hb_buffer_t *buf = hb_buffer_create();

	// Set buffer to LTR direction, common script and default language
	hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
	hb_buffer_set_script(buf, HB_SCRIPT_COMMON);
	hb_buffer_set_language(buf, hb_language_get_default());

	// Add text and layout it
	hb_buffer_add_utf8(buf, text, -1, 0, -1);
	hb_shape(context->state->hb_font, buf, NULL, 0);

	// Get buffer data
	unsigned int glyph_count = hb_buffer_get_length(buf);
	hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buf, NULL);

	double width = 0;
	for (int i = 0; i < glyph_count; ++i) {
		width += glyph_pos[i].x_advance / (64.0);
	}

	hb_buffer_destroy(buf);

	return width > max_width ? max_width / width : 1.;
}

static JSValue nx_canvas_context_2d_fill_text(JSContext *ctx,
											  JSValueConst this_val, int argc,
											  JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	double args[2];
	if (js_validate_doubles_args(ctx, argv, args, 2, 1))
		return JS_EXCEPTION;

	const char *text = JS_ToCString(ctx, argv[0]);
	if (!text)
		return JS_EXCEPTION;

	double scale = 1.;
	double font_size = context->state->font_size;

	if (argc >= 4 && JS_IsNumber(argv[3])) {
		double max_width;
		if (JS_ToFloat64(ctx, &max_width, argv[3]))
			return JS_EXCEPTION;
		scale = get_text_scale(context, text, max_width);

		if (scale != 1.) {
			set_font_size(context, font_size * scale);
		}
	}

	// Create HarfBuzz buffer
	hb_buffer_t *buf = hb_buffer_create();

	// Set buffer to LTR direction, common script and default language
	hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
	hb_buffer_set_script(buf, HB_SCRIPT_COMMON);
	hb_buffer_set_language(buf, hb_language_get_default());

	// Add text and layout it
	hb_buffer_add_utf8(buf, text, -1, 0, -1);
	hb_shape(context->state->hb_font, buf, NULL, 0);

	// Get buffer data
	unsigned int glyph_count = hb_buffer_get_length(buf);
	hb_glyph_info_t *glyph_info = hb_buffer_get_glyph_infos(buf, NULL);
	hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buf, NULL);

	// Shape glyph for Cairo
	cairo_glyph_t *cairo_glyphs = cairo_glyph_allocate(glyph_count);
	double x = 0;
	double y = 0;
	for (int i = 0; i < glyph_count; ++i) {
		cairo_glyphs[i].index = glyph_info[i].codepoint;
		cairo_glyphs[i].x = x + (glyph_pos[i].x_offset / (64.0));
		cairo_glyphs[i].y = -(y + glyph_pos[i].y_offset / (64.0));
		x += glyph_pos[i].x_advance / (64.0);
		y += glyph_pos[i].y_advance / (64.0);
	}

	// TODO: consider RTL fonts / `direction` property for START / END mode
	double alignment_offset = 0; // TEXT_ALIGN_START / TEXT_ALIGN_LEFT
	if (context->state->text_align == TEXT_ALIGN_END ||
		context->state->text_align == TEXT_ALIGN_RIGHT) {
		alignment_offset = -x;
	} else if (context->state->text_align == TEXT_ALIGN_CENTER) {
		alignment_offset = -x / 2.0;
	}

	double baseline_offset = 0; // TEXT_BASELINE_ALPHABETIC
	if (context->state->text_baseline == TEXT_BASELINE_TOP) {
		baseline_offset =
			context->state->ft_face->size->metrics.ascender / 64.0;
	} else if (context->state->text_baseline == TEXT_BASELINE_HANGING) {
		// TODO: don't know how to properly calculate this, so just pick a
		// multiplier that seems close
		baseline_offset =
			(context->state->ft_face->size->metrics.ascender / 64.0) * 0.80;
	} else if (context->state->text_baseline == TEXT_BASELINE_MIDDLE) {
		baseline_offset =
			((context->state->ft_face->size->metrics.ascender / 64.0) +
			 (context->state->ft_face->size->metrics.descender / 64.0)) /
			2.0;
	} else if (context->state->text_baseline == TEXT_BASELINE_IDEOGRAPHIC) {
		baseline_offset =
			context->state->ft_face->size->metrics.descender / 64.0;
	} else if (context->state->text_baseline == TEXT_BASELINE_BOTTOM) {
		// TODO: don't know how to properly calculate this, so just pick a
		// multiplier that seems close
		baseline_offset =
			(context->state->ft_face->size->metrics.descender / 64.0) * 2.0;
	}

	// Move glyphs to the correct positions
	for (int i = 0; i < glyph_count; ++i) {
		cairo_glyphs[i].x += args[0] + alignment_offset;
		cairo_glyphs[i].y += args[1] + baseline_offset;
	}

	if (context->state->fill_source_type == SOURCE_GRADIENT &&
		context->state->fill_gradient) {
		cairo_set_source(cr, context->state->fill_gradient);
	} else {
		cairo_set_source_rgba(cr, context->state->fill.r, context->state->fill.g,
							  context->state->fill.b,
							  context->state->fill.a *
								  context->state->global_alpha);
	}

	cairo_show_glyphs(cr, cairo_glyphs, glyph_count);

	if (scale != 1.) {
		set_font_size(context, font_size);
	}

	cairo_glyph_free(cairo_glyphs);
	hb_buffer_destroy(buf);
	JS_FreeCString(ctx, text);

	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_stroke_text(JSContext *ctx,
												JSValueConst this_val, int argc,
												JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	double args[2];
	if (js_validate_doubles_args(ctx, argv, args, 2, 1))
		return JS_EXCEPTION;

	const char *text = JS_ToCString(ctx, argv[0]);
	if (!text)
		return JS_EXCEPTION;

	save_path(context);

	double scale = 1.;
	double font_size = context->state->font_size;

	if (argc >= 4 && JS_IsNumber(argv[3])) {
		double max_width;
		if (JS_ToFloat64(ctx, &max_width, argv[3]))
			return JS_EXCEPTION;
		scale = get_text_scale(context, text, max_width);

		if (scale != 1.) {
			set_font_size(context, font_size * scale);
		}
	}

	// Create HarfBuzz buffer
	hb_buffer_t *buf = hb_buffer_create();

	// Set buffer to LTR direction, common script and default language
	hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
	hb_buffer_set_script(buf, HB_SCRIPT_COMMON);
	hb_buffer_set_language(buf, hb_language_get_default());

	// Add text and layout it
	hb_buffer_add_utf8(buf, text, -1, 0, -1);
	hb_shape(context->state->hb_font, buf, NULL, 0);

	// Get buffer data
	unsigned int glyph_count = hb_buffer_get_length(buf);
	hb_glyph_info_t *glyph_info = hb_buffer_get_glyph_infos(buf, NULL);
	hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buf, NULL);

	// Shape glyph for Cairo
	cairo_glyph_t *cairo_glyphs = cairo_glyph_allocate(glyph_count);
	double x = 0;
	double y = 0;
	for (int i = 0; i < glyph_count; ++i) {
		cairo_glyphs[i].index = glyph_info[i].codepoint;
		cairo_glyphs[i].x = x + (glyph_pos[i].x_offset / (64.0));
		cairo_glyphs[i].y = -(y + glyph_pos[i].y_offset / (64.0));
		x += glyph_pos[i].x_advance / (64.0);
		y += glyph_pos[i].y_advance / (64.0);
	}

	// TODO: consider RTL fonts / `direction` property for START / END mode
	double alignment_offset = 0; // TEXT_ALIGN_START / TEXT_ALIGN_LEFT
	if (context->state->text_align == TEXT_ALIGN_END ||
		context->state->text_align == TEXT_ALIGN_RIGHT) {
		alignment_offset = -x;
	} else if (context->state->text_align == TEXT_ALIGN_CENTER) {
		alignment_offset = -x / 2.0;
	}

	double baseline_offset = 0; // TEXT_BASELINE_ALPHABETIC
	if (context->state->text_baseline == TEXT_BASELINE_TOP) {
		baseline_offset =
			context->state->ft_face->size->metrics.ascender / 64.0;
	} else if (context->state->text_baseline == TEXT_BASELINE_HANGING) {
		// TODO: don't know how to properly calculate this, so just pick a
		// multiplier that seems close
		baseline_offset =
			(context->state->ft_face->size->metrics.ascender / 64.0) * 0.80;
	} else if (context->state->text_baseline == TEXT_BASELINE_MIDDLE) {
		baseline_offset =
			((context->state->ft_face->size->metrics.ascender / 64.0) +
			 (context->state->ft_face->size->metrics.descender / 64.0)) /
			2.0;
	} else if (context->state->text_baseline == TEXT_BASELINE_IDEOGRAPHIC) {
		baseline_offset =
			context->state->ft_face->size->metrics.descender / 64.0;
	} else if (context->state->text_baseline == TEXT_BASELINE_BOTTOM) {
		// TODO: don't know how to properly calculate this, so just pick a
		// multiplier that seems close
		baseline_offset =
			(context->state->ft_face->size->metrics.descender / 64.0) * 2.0;
	}

	// Move glyphs to the correct positions
	for (int i = 0; i < glyph_count; ++i) {
		cairo_glyphs[i].x += args[0] + alignment_offset;
		cairo_glyphs[i].y += args[1] + baseline_offset;
	}

	// Draw the text onto the Cairo surface
	cairo_glyph_path(cr, cairo_glyphs, glyph_count);

	stroke(context, false);

	if (scale != 1.) {
		set_font_size(context, font_size);
	}

	restore_path(context);

	cairo_glyph_free(cairo_glyphs);
	hb_buffer_destroy(buf);
	JS_FreeCString(ctx, text);

	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_measure_text(JSContext *ctx,
												 JSValueConst this_val,
												 int argc, JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	const char *text = JS_ToCString(ctx, argv[0]);

	// Create HarfBuzz buffer
	hb_buffer_t *buf = hb_buffer_create();

	// Set buffer to LTR direction, common script and default language
	hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
	hb_buffer_set_script(buf, HB_SCRIPT_COMMON);
	hb_buffer_set_language(buf, hb_language_get_default());

	// Add text and layout it
	hb_buffer_add_utf8(buf, text, -1, 0, -1);
	hb_shape(context->state->hb_font, buf, NULL, 0);

	// Get buffer data
	unsigned int glyph_count = hb_buffer_get_length(buf);
	hb_glyph_position_t *glyph_pos = hb_buffer_get_glyph_positions(buf, NULL);

	double width = 0;
	// Calculate the width of the text
	for (int i = 0; i < glyph_count; ++i) {
		width += glyph_pos[i].x_advance / 64.0;
	}

	// Create the TextMetrics object
	JSValue metrics = JS_NewObject(ctx);

	// Set the width property
	JS_SetPropertyStr(ctx, metrics, "width", JS_NewFloat64(ctx, width));

	// Set the rest of the properties to 0 for now
	JS_SetPropertyStr(ctx, metrics, "actualBoundingBoxLeft",
					  JS_NewFloat64(ctx, 0));
	JS_SetPropertyStr(ctx, metrics, "actualBoundingBoxRight",
					  JS_NewFloat64(ctx, 0));
	JS_SetPropertyStr(ctx, metrics, "fontBoundingBoxAscent",
					  JS_NewFloat64(ctx, 0));
	JS_SetPropertyStr(ctx, metrics, "fontBoundingBoxDescent",
					  JS_NewFloat64(ctx, 0));
	JS_SetPropertyStr(ctx, metrics, "actualBoundingBoxAscent",
					  JS_NewFloat64(ctx, 0));
	JS_SetPropertyStr(ctx, metrics, "actualBoundingBoxDescent",
					  JS_NewFloat64(ctx, 0));
	JS_SetPropertyStr(ctx, metrics, "emHeightAscent", JS_NewFloat64(ctx, 0));
	JS_SetPropertyStr(ctx, metrics, "emHeightDescent", JS_NewFloat64(ctx, 0));
	JS_SetPropertyStr(ctx, metrics, "hangingBaseline", JS_NewFloat64(ctx, 0));
	JS_SetPropertyStr(ctx, metrics, "alphabeticBaseline",
					  JS_NewFloat64(ctx, 0));
	JS_SetPropertyStr(ctx, metrics, "ideographicBaseline",
					  JS_NewFloat64(ctx, 0));

	JS_FreeCString(ctx, text);
	hb_buffer_destroy(buf);

	return metrics;
}

static void js_free_array_buffer(JSRuntime *rt, void *opaque, void *ptr) {
	js_free_rt(rt, ptr);
}

static JSValue nx_canvas_context_2d_put_image_data(JSContext *ctx,
												   JSValueConst this_val,
												   int argc,
												   JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;

	int sx = 0, sy = 0, sw = 0, sh = 0, dx, dy, image_data_width,
		image_data_height, rows, cols;
	size_t src_offset, src_length, bytes_per_element;

	JSValue image_data_array_value = JS_GetPropertyStr(ctx, argv[0], "data");
	JSValue image_data_width_value = JS_GetPropertyStr(ctx, argv[0], "width");
	JSValue image_data_height_value = JS_GetPropertyStr(ctx, argv[0], "height");
	JSValue image_data_buffer_value =
		JS_GetTypedArrayBuffer(ctx, image_data_array_value, &src_offset,
							   &src_length, &bytes_per_element);
	JS_FreeValue(ctx, image_data_array_value);
	if (JS_IsException(image_data_buffer_value))
		return image_data_buffer_value;

	uint8_t *src = JS_GetArrayBuffer(ctx, &src_length, image_data_buffer_value);
	JS_FreeValue(ctx, image_data_buffer_value);
	src += src_offset;

	if (JS_ToInt32(ctx, &image_data_width, image_data_width_value) ||
		JS_ToInt32(ctx, &image_data_height, image_data_height_value) ||
		JS_ToInt32(ctx, &dx, argv[1]) || JS_ToInt32(ctx, &dy, argv[2])) {
		return JS_EXCEPTION;
	}

	uint8_t *dst = context->canvas->data;
	int dstStride = context->canvas->width * 4;
	int srcStride = image_data_width * 4;

	switch (argc) {
	case 3:
		// imageData, dx, dy
		sw = image_data_width;
		sh = image_data_height;
		break;
	case 7:
		// imageData, dx, dy, sx, sy, sw, sh
		if (JS_ToInt32(ctx, &sx, argv[3]) || JS_ToInt32(ctx, &sy, argv[4]) ||
			JS_ToInt32(ctx, &sw, argv[5]) || JS_ToInt32(ctx, &sh, argv[6])) {
			return JS_EXCEPTION;
		}
		// fix up negative height, width
		if (sw < 0)
			sx += sw, sw = -sw;
		if (sh < 0)
			sy += sh, sh = -sh;
		// clamp the left edge
		if (sx < 0)
			sw += sx, sx = 0;
		if (sy < 0)
			sh += sy, sy = 0;
		// clamp the right edge
		if (sx + sw > image_data_width)
			sw = image_data_width - sx;
		if (sy + sh > image_data_height)
			sh = image_data_height - sy;
		// start destination at source offset
		dx += sx;
		dy += sy;
		break;
	default:
		JS_ThrowTypeError(ctx, "Invalid argument count: %d", argc);
		return JS_EXCEPTION;
	}

	// chop off outlying source data
	if (dx < 0)
		sw += dx, sx -= dx, dx = 0;
	if (dy < 0)
		sh += dy, sy -= dy, dy = 0;

	// clamp width at canvas size
	cols = min(sw, context->canvas->width - dx);
	rows = min(sh, context->canvas->height - dy);

	if (cols <= 0 || rows <= 0)
		return JS_UNDEFINED;

	src += sy * srcStride + sx * 4;
	dst += dstStride * dy + 4 * dx;
	for (int y = 0; y < rows; ++y) {
		uint8_t *dstRow = dst;
		uint8_t *srcRow = src;
		for (int x = 0; x < cols; ++x) {
			// rgba
			uint8_t r = *srcRow++;
			uint8_t g = *srcRow++;
			uint8_t b = *srcRow++;
			uint8_t a = *srcRow++;

			// argb
			// performance optimization: fully transparent/opaque pixels can be
			// processed more efficiently.
			if (a == 0) {
				*dstRow++ = 0;
				*dstRow++ = 0;
				*dstRow++ = 0;
				*dstRow++ = 0;
			} else if (a == 255) {
				*dstRow++ = b;
				*dstRow++ = g;
				*dstRow++ = r;
				*dstRow++ = a;
			} else {
				float alpha = (float)a / 255;
				*dstRow++ = b * alpha;
				*dstRow++ = g * alpha;
				*dstRow++ = r * alpha;
				*dstRow++ = a;
			}
		}
		dst += dstStride;
		src += srcStride;
	}

	cairo_surface_mark_dirty_rectangle(context->canvas->surface, dx, dy, cols,
									   rows);

	return JS_UNDEFINED;
}

/*
 * Take a transform matrix and return its components
 * 0: angle, 1: scaleX, 2: scaleY, 3: skewX, 4: translateX, 5: translateY
 */
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

static JSValue nx_canvas_context_2d_draw_image(JSContext *ctx,
											   JSValueConst this_val, int argc,
											   JSValueConst *argv) {
	if (argc != 3 && argc != 5 && argc != 9) {
		JS_ThrowTypeError(ctx, "Invalid arguments");
		return JS_EXCEPTION;
	}

	double args[8];
	if (js_validate_doubles_args(ctx, argv, args, argc - 1, 1))
		return JS_EXCEPTION;

	CANVAS_CONTEXT_THIS;

	double sx = 0, sy = 0, sw = 0, sh = 0, dx = 0, dy = 0, dw = 0, dh = 0,
		   source_w = 0, source_h = 0;
	cairo_surface_t *surface;

	nx_image_t *img = nx_get_image(ctx, argv[0]);
	if (img) {
		surface = img->surface;
		source_w = sw = img->width;
		source_h = sh = img->height;
	} else {
		nx_canvas_t *canvas = nx_get_canvas(ctx, argv[0]);
		if (!canvas) {
			JS_ThrowTypeError(ctx, "Image or Canvas expected");
			return JS_EXCEPTION;
		}
		surface = canvas->surface;
		source_w = sw = canvas->width;
		source_h = sh = canvas->height;
	}

	// Arguments
	switch (argc) {
	case 9:
		// img, sx, sy, sw, sh, dx, dy, dw, dh
		sx = args[0];
		sy = args[1];
		sw = args[2];
		sh = args[3];
		dx = args[4];
		dy = args[5];
		dw = args[6];
		dh = args[7];
		break;
	case 5:
		// img, dx, dy, dw, dh
		dx = args[0];
		dy = args[1];
		dw = args[2];
		dh = args[3];
		break;
	case 3:
		// img, dx, dy
		dx = args[0];
		dy = args[1];
		dw = sw;
		dh = sh;
		break;
	}

	if (!(sw && sh && dw && dh))
		return JS_UNDEFINED;

	// Start draw
	cairo_save(cr);

	cairo_matrix_t matrix;
	double transforms[6];
	cairo_get_matrix(cr, &matrix);
	decompose_matrix(matrix, transforms);
	// extract the scale value from the current transform so that we know how
	// many pixels we need for our extra canvas in the drawImage operation.
	double current_scale_x = abs(transforms[1]);
	double current_scale_y = abs(transforms[2]);
	double extra_dx = 0;
	double extra_dy = 0;
	double fx = dw / sw * current_scale_x; // transforms[1] is scale on X
	double fy = dh / sh * current_scale_y; // transforms[2] is scale on X
	bool needScale = dw != sw || dh != sh;
	bool needCut = sw != source_w || sh != source_h || sx < 0 || sy < 0;
	bool sameCanvas = surface == context->canvas->surface;
	bool needsExtraSurface = sameCanvas || needCut || needScale;
	cairo_surface_t *surfTemp = NULL;
	cairo_t *ctxTemp = NULL;

	if (needsExtraSurface) {
		// we want to create the extra surface as small as possible.
		// fx and fy are the total scaling we need to apply to sw, sh.
		// from sw and sh we want to remove the part that is outside the
		// source_w and soruce_h
		double real_w = sw;
		double real_h = sh;
		double translate_x = 0;
		double translate_y = 0;
		// if sx or sy are negative, a part of the area represented by sw and sh
		// is empty because there are empty pixels, so we cut it out. On the
		// other hand if sx or sy are positive, but sw and sh extend outside the
		// real source pixels, we cut the area in that case too.
		if (sx < 0) {
			extra_dx = -sx * fx;
			real_w = sw + sx;
		} else if (sx + sw > source_w) {
			real_w = sw - (sx + sw - source_w);
		}
		if (sy < 0) {
			extra_dy = -sy * fy;
			real_h = sh + sy;
		} else if (sy + sh > source_h) {
			real_h = sh - (sy + sh - source_h);
		}
		// if after cutting we are still bigger than source pixels, we restrict
		// again
		if (real_w > source_w) {
			real_w = source_w;
		}
		if (real_h > source_h) {
			real_h = source_h;
		}
		// TODO: find a way to limit the surfTemp to real_w and real_h if fx and
		// fy are bigger than 1. there are no more pixel than the one available
		// in the source, no need to create a bigger surface.
		surfTemp = cairo_image_surface_create(
			CAIRO_FORMAT_ARGB32, round(real_w * fx), round(real_h * fy));
		ctxTemp = cairo_create(surfTemp);
		cairo_scale(ctxTemp, fx, fy);
		if (sx > 0) {
			translate_x = sx;
		}
		if (sy > 0) {
			translate_y = sy;
		}
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

	// TODO: Support shadow
	// apply shadow if there is one
	// if (context->hasShadow()) {
	//  if(context->state->shadowBlur) {
	//    // we need to create a new surface in order to blur
	//    int pad = context->state->shadowBlur * 2;
	//    cairo_surface_t *shadow_surface =
	//    cairo_image_surface_create(CAIRO_FORMAT_ARGB32, dw + 2 * pad, dh + 2 *
	//    pad); cairo_t *shadow_context = cairo_create(shadow_surface);

	//    // mask and blur
	//    context->setSourceRGBA(shadow_context, context->state->shadow);
	//    cairo_mask_surface(shadow_context, surface, pad, pad);
	//    context->blur(shadow_surface, context->state->shadowBlur);

	//    // paint
	//    // @note: ShadowBlur looks different in each browser. This
	//    implementation matches chrome as close as possible.
	//    //        The 1.4 offset comes from visual tests with Chrome. I have
	//    read the spec and part of the shadowBlur
	//    //        implementation, and its not immediately clear why an offset
	//    is necessary, but without it, the result
	//    //        in chrome is different.
	//    cairo_set_source_surface(ctx, shadow_surface,
	//      dx + context->state->shadowOffsetX - pad + 1.4,
	//      dy + context->state->shadowOffsetY - pad + 1.4);
	//    cairo_paint(ctx);
	//    // cleanup
	//    cairo_destroy(shadow_context);
	//    cairo_surface_destroy(shadow_surface);
	//  } else {
	//    context->setSourceRGBA(context->state->shadow);
	//    cairo_mask_surface(ctx, surface,
	//      dx + (context->state->shadowOffsetX),
	//      dy + (context->state->shadowOffsetY));
	//  }
	//}

	double scaled_dx = dx;
	double scaled_dy = dy;

	if (needsExtraSurface && (current_scale_x != 1 || current_scale_y != 1)) {
		// in this case our surface contains already current_scale_x, we need to
		// scale back
		cairo_scale(cr, 1 / current_scale_x, 1 / current_scale_y);
		scaled_dx *= current_scale_x;
		scaled_dy *= current_scale_y;
	}

	// Paint
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

	return JS_UNDEFINED;
}

static void
finalizer_canvas_context_2d_state(JSRuntime *rt,
								  nx_canvas_context_2d_state_t *state) {
	// printf("finalizer_canvas_context_2d_state\n");
	if (state->next) {
		finalizer_canvas_context_2d_state(rt, state->next);
	}
	if (state->font_string) {
		free((void *)state->font_string);
	}
	if (state->fill_gradient) {
		cairo_pattern_destroy(state->fill_gradient);
	}
	if (state->stroke_gradient) {
		cairo_pattern_destroy(state->stroke_gradient);
	}
	js_free_rt(rt, state);
}

static void finalizer_canvas_context_2d(JSRuntime *rt, JSValue val) {
	// printf("finalizer_canvas_context_2d\n");
	nx_canvas_context_2d_t *context =
		JS_GetOpaque(val, nx_canvas_context_class_id);
	if (context) {
		cairo_destroy(context->ctx);
		finalizer_canvas_context_2d_state(rt, context->state);
		js_free_rt(rt, context);
	}
}

nx_canvas_context_2d_t *nx_get_canvas_context_2d(JSContext *ctx,
												 JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_canvas_context_class_id);
}

nx_canvas_t *nx_get_canvas(JSContext *ctx, JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_canvas_class_id);
}

static JSValue nx_canvas_new(JSContext *ctx, JSValueConst this_val, int argc,
							 JSValueConst *argv) {
	int width;
	int height;
	if (JS_ToInt32(ctx, &width, argv[0]))
		return JS_EXCEPTION;
	if (JS_ToInt32(ctx, &height, argv[1]))
		return JS_EXCEPTION;

	size_t buf_size = width * height * 4;
	uint8_t *buffer = js_mallocz(ctx, buf_size);
	if (!buffer)
		return JS_EXCEPTION;

	nx_canvas_t *context = js_mallocz(ctx, sizeof(nx_canvas_t));
	if (!context)
		return JS_EXCEPTION;

	JSValue obj = JS_NewObjectClass(ctx, nx_canvas_class_id);
	if (JS_IsException(obj)) {
		js_free(ctx, context);
		return obj;
	}

	// On Switch, the byte order seems to be BGRA
	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		buffer, CAIRO_FORMAT_ARGB32, width, height, width * 4);

	context->width = width;
	context->height = height;
	context->data = buffer;
	context->surface = surface;

	JS_SetOpaque(obj, context);
	return obj;
}

static JSValue nx_canvas_get_width(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_canvas_t *canvas = nx_get_canvas(ctx, this_val);
	if (!canvas)
		return JS_EXCEPTION;
	return JS_NewUint32(ctx, canvas->width);
}

static JSValue nx_canvas_set_width(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_canvas_t *canvas = nx_get_canvas(ctx, this_val);
	if (!canvas)
		return JS_EXCEPTION;
	uint32_t new_width;
	if (JS_ToUint32(ctx, &new_width, argv[0]))
		return JS_EXCEPTION;
	// Per spec, setting width always resets the canvas, even to the same value
	canvas->width = new_width;
	if (canvas->surface) {
		cairo_surface_destroy(canvas->surface);
		canvas->surface = NULL;
	}
	if (canvas->data) {
		js_free(ctx, canvas->data);
		canvas->data = NULL;
	}
	return JS_UNDEFINED;
}

static JSValue nx_canvas_get_height(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_canvas_t *canvas = nx_get_canvas(ctx, this_val);
	return JS_NewUint32(ctx, canvas->height);
}

static JSValue nx_canvas_set_height(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_canvas_t *canvas = nx_get_canvas(ctx, this_val);
	if (!canvas)
		return JS_EXCEPTION;
	uint32_t new_height;
	if (JS_ToUint32(ctx, &new_height, argv[0]))
		return JS_EXCEPTION;
	// Per spec, setting height always resets the canvas, even to the same value
	canvas->height = new_height;
	if (canvas->surface) {
		cairo_surface_destroy(canvas->surface);
		canvas->surface = NULL;
	}
	if (canvas->data) {
		js_free(ctx, canvas->data);
		canvas->data = NULL;
	}
	return JS_UNDEFINED;
}

static JSValue nx_canvas_init_class(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_GETSET(proto, "width", nx_canvas_get_width, nx_canvas_set_width);
	NX_DEF_GETSET(proto, "height", nx_canvas_get_height, nx_canvas_set_height);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_get_fill_style(JSContext *ctx,
												   JSValueConst this_val,
												   int argc,
												   JSValueConst *argv) {
	CANVAS_CONTEXT_ARGV0;
	JSValue rgba = JS_NewArray(ctx);
	JS_SetPropertyUint32(ctx, rgba, 0,
						 JS_NewInt32(ctx, context->state->fill.r * 255));
	JS_SetPropertyUint32(ctx, rgba, 1,
						 JS_NewInt32(ctx, context->state->fill.g * 255));
	JS_SetPropertyUint32(ctx, rgba, 2,
						 JS_NewInt32(ctx, context->state->fill.b * 255));
	JS_SetPropertyUint32(ctx, rgba, 3,
						 JS_NewFloat64(ctx, context->state->fill.a));
	return rgba;
}

static JSValue nx_canvas_context_2d_set_fill_style(JSContext *ctx,
												   JSValueConst this_val,
												   int argc,
												   JSValueConst *argv) {
	CANVAS_CONTEXT_ARGV0;
	double args[4];
	if (js_validate_doubles_args(ctx, argv, args, 4, 1))
		return JS_EXCEPTION;
	context->state->fill.r = args[0] / 255.;
	context->state->fill.g = args[1] / 255.;
	context->state->fill.b = args[2] / 255.;
	context->state->fill.a = args[3];
	context->state->fill_source_type = SOURCE_RGBA;
	if (context->state->fill_gradient) {
		cairo_pattern_destroy(context->state->fill_gradient);
		context->state->fill_gradient = NULL;
	}
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_get_stroke_style(JSContext *ctx,
													 JSValueConst this_val,
													 int argc,
													 JSValueConst *argv) {
	CANVAS_CONTEXT_ARGV0;
	JSValue rgba = JS_NewArray(ctx);
	JS_SetPropertyUint32(ctx, rgba, 0,
						 JS_NewInt32(ctx, context->state->stroke.r * 255));
	JS_SetPropertyUint32(ctx, rgba, 1,
						 JS_NewInt32(ctx, context->state->stroke.g * 255));
	JS_SetPropertyUint32(ctx, rgba, 2,
						 JS_NewInt32(ctx, context->state->stroke.b * 255));
	JS_SetPropertyUint32(ctx, rgba, 3,
						 JS_NewFloat64(ctx, context->state->stroke.a));
	return rgba;
}

static JSValue nx_canvas_context_2d_set_stroke_style(JSContext *ctx,
													 JSValueConst this_val,
													 int argc,
													 JSValueConst *argv) {
	CANVAS_CONTEXT_ARGV0;
	double args[4];
	if (js_validate_doubles_args(ctx, argv, args, 4, 1))
		return JS_EXCEPTION;
	context->state->stroke.r = args[0] / 255.;
	context->state->stroke.g = args[1] / 255.;
	context->state->stroke.b = args[2] / 255.;
	context->state->stroke.a = args[3];
	context->state->stroke_source_type = SOURCE_RGBA;
	if (context->state->stroke_gradient) {
		cairo_pattern_destroy(context->state->stroke_gradient);
		context->state->stroke_gradient = NULL;
	}
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_begin_path(JSContext *ctx,
											   JSValueConst this_val, int argc,
											   JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	cairo_new_path(cr);
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_close_path(JSContext *ctx,
											   JSValueConst this_val, int argc,
											   JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	cairo_close_path(cr);
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_clip(JSContext *ctx, JSValueConst this_val,
										 int argc, JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	set_fill_rule(ctx, argv[0], cr);
	cairo_clip_preserve(cr);
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_fill(JSContext *ctx, JSValueConst this_val,
										 int argc, JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	JSValue path = JS_NULL;
	JSValue fill_rule = JS_NULL;
	if (argc == 1) {
		if (JS_IsObject(argv[0])) {
			path = argv[0];
		} else if (JS_IsString(argv[0])) {
			fill_rule = argv[0];
		} else if (!JS_IsUndefined(argv[0])) {
			return JS_ThrowTypeError(ctx,
									 "Expected Path2D or string at index 0");
		}
	} else if (argc == 2) {
		if (JS_IsObject(argv[0])) {
			path = argv[0];
		} else {
			return JS_ThrowTypeError(ctx, "Expected Path2D at index 0");
		}
		if (JS_IsString(argv[1])) {
			fill_rule = argv[1];
		} else if (!JS_IsUndefined(argv[1])) {
			return JS_ThrowTypeError(ctx, "Expected string at index 1");
		}
	}
	if (!JS_IsNull(fill_rule)) {
		set_fill_rule(ctx, argv[0], cr);
	}
	if (JS_IsNull(path)) {
		fill(context, true);
	} else {
		save_path(context);
		apply_path(ctx, this_val, path);
		fill(context, false);
		restore_path(context);
	}
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_stroke(JSContext *ctx,
										   JSValueConst this_val, int argc,
										   JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	JSValue path = JS_NULL;
	if (argc == 1) {
		if (JS_IsObject(argv[0])) {
			path = argv[0];
		} else {
			return JS_ThrowTypeError(ctx, "Expected Path2D at index 0");
		}
	}
	if (JS_IsNull(path)) {
		stroke(context, true);
	} else {
		save_path(context);
		apply_path(ctx, this_val, path);
		stroke(context, false);
		restore_path(context);
	}
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_save(JSContext *ctx, JSValueConst this_val,
										 int argc, JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	cairo_save(cr);
	nx_canvas_context_2d_state_t *state =
		js_mallocz(ctx, sizeof(nx_canvas_context_2d_state_t));
	memcpy(state, context->state, sizeof(nx_canvas_context_2d_state_t));
	state->next = context->state;
	state->font = context->state->font;
	if (context->state->font_string) {
		state->font_string = strdup(context->state->font_string);
	}
	if (state->fill_gradient) {
		cairo_pattern_reference(state->fill_gradient);
	}
	if (state->stroke_gradient) {
		cairo_pattern_reference(state->stroke_gradient);
	}
	context->state = state;
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_restore(JSContext *ctx,
											JSValueConst this_val, int argc,
											JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	if (context->state->next) {
		cairo_restore(cr);
		nx_canvas_context_2d_state_t *prev = context->state;
		context->state = prev->next;
		if (prev->font_string) {
			free((void *)prev->font_string);
		}
		if (prev->fill_gradient) {
			cairo_pattern_destroy(prev->fill_gradient);
		}
		if (prev->stroke_gradient) {
			cairo_pattern_destroy(prev->stroke_gradient);
		}
		js_free(ctx, prev);

		nx_font_face_t *face = nx_get_font_face(ctx, context->state->font);
		if (face) {
			cairo_set_font_face(cr, face->cairo_font);
			set_font_size(context, context->state->font_size);
		}
	}
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_fill_rect(JSContext *ctx,
											  JSValueConst this_val, int argc,
											  JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	RECT_ARGS;
	if (width && height) {
		save_path(context);
		cairo_rectangle(cr, x, y, width, height);

		// TODO: support gradient / pattern
		fill(context, false);

		restore_path(context);
	}
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_get_line_width(JSContext *ctx,
												   JSValueConst this_val,
												   int argc,
												   JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	return JS_NewFloat64(ctx, cairo_get_line_width(cr));
}

static JSValue nx_canvas_context_2d_set_line_width(JSContext *ctx,
												   JSValueConst this_val,
												   int argc,
												   JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	double n;
	if (JS_ToFloat64(ctx, &n, argv[0]))
		return JS_EXCEPTION;
	cairo_set_line_width(cr, n);
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_get_line_join(JSContext *ctx,
												  JSValueConst this_val,
												  int argc,
												  JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	const char *join;
	switch (cairo_get_line_join(cr)) {
	case CAIRO_LINE_JOIN_BEVEL:
		join = "bevel";
		break;
	case CAIRO_LINE_JOIN_ROUND:
		join = "round";
		break;
	default:
		join = "miter";
	}
	return JS_NewString(ctx, join);
}

static JSValue nx_canvas_context_2d_set_line_join(JSContext *ctx,
												  JSValueConst this_val,
												  int argc,
												  JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	const char *type = JS_ToCString(ctx, argv[0]);
	if (!type)
		return JS_EXCEPTION;
	cairo_line_join_t line_join;
	if (0 == strcmp("round", type)) {
		line_join = CAIRO_LINE_JOIN_ROUND;
	} else if (0 == strcmp("bevel", type)) {
		line_join = CAIRO_LINE_JOIN_BEVEL;
	} else {
		line_join = CAIRO_LINE_JOIN_MITER;
	}
	JS_FreeCString(ctx, type);
	cairo_set_line_join(cr, line_join);
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_get_line_dash_offset(JSContext *ctx,
														 JSValueConst this_val,
														 int argc,
														 JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	double offset;
	cairo_get_dash(cr, NULL, &offset);
	return JS_NewFloat64(ctx, offset);
}

static JSValue nx_canvas_context_2d_set_line_dash_offset(JSContext *ctx,
														 JSValueConst this_val,
														 int argc,
														 JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	double offset;
	if (JS_ToFloat64(ctx, &offset, argv[0]))
		return JS_EXCEPTION;
	int num_dashes = cairo_get_dash_count(cr);
	double dashes[num_dashes];
	cairo_get_dash(cr, dashes, NULL);
	cairo_set_dash(cr, dashes, num_dashes, offset);
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_get_line_cap(JSContext *ctx,
												 JSValueConst this_val,
												 int argc, JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	const char *cap;
	switch (cairo_get_line_cap(cr)) {
	case CAIRO_LINE_CAP_ROUND:
		cap = "round";
		break;
	case CAIRO_LINE_CAP_SQUARE:
		cap = "square";
		break;
	default:
		cap = "butt";
	}
	return JS_NewString(ctx, cap);
}

static JSValue nx_canvas_context_2d_set_line_cap(JSContext *ctx,
												 JSValueConst this_val,
												 int argc, JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	const char *type = JS_ToCString(ctx, argv[0]);
	if (!type)
		return JS_EXCEPTION;
	cairo_line_cap_t line_cap;
	if (0 == strcmp("round", type)) {
		line_cap = CAIRO_LINE_CAP_ROUND;
	} else if (0 == strcmp("square", type)) {
		line_cap = CAIRO_LINE_CAP_SQUARE;
	} else {
		line_cap = CAIRO_LINE_CAP_BUTT;
	}
	JS_FreeCString(ctx, type);
	cairo_set_line_cap(cr, line_cap);
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_get_line_dash(JSContext *ctx,
												  JSValueConst this_val,
												  int argc,
												  JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	int count = cairo_get_dash_count(cr);
	double dashes[count];
	cairo_get_dash(cr, dashes, NULL);

	JSValue array = JS_NewArray(ctx);
	for (int i = 0; i < count; i++) {
		JS_SetPropertyUint32(ctx, array, i, JS_NewFloat64(ctx, dashes[i]));
	}
	return array;
}

static JSValue nx_canvas_context_2d_set_line_dash(JSContext *ctx,
												  JSValueConst this_val,
												  int argc,
												  JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	JSValue length_val = JS_GetPropertyStr(ctx, argv[0], "length");
	uint32_t length;
	if (JS_ToUint32(ctx, &length, length_val)) {
		JS_FreeValue(ctx, length_val);
		return JS_EXCEPTION;
	}
	JS_FreeValue(ctx, length_val);
	uint32_t num_dashes = length & 1 ? length * 2 : length;
	uint32_t zero_dashes = 0;
	double dashes[num_dashes];
	for (uint32_t i = 0; i < num_dashes; i++) {
		if (JS_ToFloat64(ctx, &dashes[i],
						 JS_GetPropertyUint32(ctx, argv[0], i % length))) {
			return JS_UNDEFINED;
		}
		if (dashes[i] == 0)
			zero_dashes++;
	}
	double offset;
	cairo_get_dash(cr, NULL, &offset);
	if (zero_dashes == num_dashes) {
		cairo_set_dash(cr, NULL, 0, offset);
	} else {
		cairo_set_dash(cr, dashes, num_dashes, offset);
	}
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_get_global_alpha(JSContext *ctx,
													 JSValueConst this_val,
													 int argc,
													 JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	return JS_NewFloat64(ctx, context->state->global_alpha);
}

static JSValue nx_canvas_context_2d_set_global_alpha(JSContext *ctx,
													 JSValueConst this_val,
													 int argc,
													 JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	double value;
	if (JS_ToFloat64(ctx, &value, argv[0]))
		return JS_EXCEPTION;
	if (value >= 0 && value <= 1) {
		context->state->global_alpha = value;
	}
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_get_global_composite_operation(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	const char *op;
	switch (cairo_get_operator(cr)) {
	// composite modes:
	case CAIRO_OPERATOR_CLEAR:
		op = "clear";
		break;
	case CAIRO_OPERATOR_SOURCE:
		op = "copy";
		break;
	case CAIRO_OPERATOR_DEST:
		op = "destination";
		break;
	case CAIRO_OPERATOR_OVER:
		op = "source-over";
		break;
	case CAIRO_OPERATOR_DEST_OVER:
		op = "destination-over";
		break;
	case CAIRO_OPERATOR_IN:
		op = "source-in";
		break;
	case CAIRO_OPERATOR_DEST_IN:
		op = "destination-in";
		break;
	case CAIRO_OPERATOR_OUT:
		op = "source-out";
		break;
	case CAIRO_OPERATOR_DEST_OUT:
		op = "destination-out";
		break;
	case CAIRO_OPERATOR_ATOP:
		op = "source-atop";
		break;
	case CAIRO_OPERATOR_DEST_ATOP:
		op = "destination-atop";
		break;
	case CAIRO_OPERATOR_XOR:
		op = "xor";
		break;
	case CAIRO_OPERATOR_ADD:
		op = "lighter";
		break;
	// blend modes:
	// Note: "source-over" and "normal" are synonyms. Chrome and FF both report
	// "source-over" after setting gCO to "normal".
	// case CAIRO_OPERATOR_OVER: op = "normal";
	case CAIRO_OPERATOR_MULTIPLY:
		op = "multiply";
		break;
	case CAIRO_OPERATOR_SCREEN:
		op = "screen";
		break;
	case CAIRO_OPERATOR_OVERLAY:
		op = "overlay";
		break;
	case CAIRO_OPERATOR_DARKEN:
		op = "darken";
		break;
	case CAIRO_OPERATOR_LIGHTEN:
		op = "lighten";
		break;
	case CAIRO_OPERATOR_COLOR_DODGE:
		op = "color-dodge";
		break;
	case CAIRO_OPERATOR_COLOR_BURN:
		op = "color-burn";
		break;
	case CAIRO_OPERATOR_HARD_LIGHT:
		op = "hard-light";
		break;
	case CAIRO_OPERATOR_SOFT_LIGHT:
		op = "soft-light";
		break;
	case CAIRO_OPERATOR_DIFFERENCE:
		op = "difference";
		break;
	case CAIRO_OPERATOR_EXCLUSION:
		op = "exclusion";
		break;
	case CAIRO_OPERATOR_HSL_HUE:
		op = "hue";
		break;
	case CAIRO_OPERATOR_HSL_SATURATION:
		op = "saturation";
		break;
	case CAIRO_OPERATOR_HSL_COLOR:
		op = "color";
		break;
	case CAIRO_OPERATOR_HSL_LUMINOSITY:
		op = "luminosity";
		break;
	// non-standard:
	case CAIRO_OPERATOR_SATURATE:
		op = "saturate";
		break;
	default:
		op = "source-over";
	}

	return JS_NewString(ctx, op);
}

static JSValue nx_canvas_context_2d_set_global_composite_operation(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	int op = -1;
	const char *str = JS_ToCString(ctx, argv[0]);
	if (!str)
		return JS_EXCEPTION;
	if (strcmp(str, "clear") == 0) {
		// composite modes:
		op = CAIRO_OPERATOR_CLEAR;
	} else if (strcmp(str, "copy") == 0) {
		op = CAIRO_OPERATOR_SOURCE;
	} else if (strcmp(str, "destination") == 0) {
		op = CAIRO_OPERATOR_DEST; // this seems to have been omitted from the
								  // spec
	} else if (strcmp(str, "source-over") == 0) {
		op = CAIRO_OPERATOR_OVER;
	} else if (strcmp(str, "destination-over") == 0) {
		op = CAIRO_OPERATOR_DEST_OVER;
	} else if (strcmp(str, "source-in") == 0) {
		op = CAIRO_OPERATOR_IN;
	} else if (strcmp(str, "destination-in") == 0) {
		op = CAIRO_OPERATOR_DEST_IN;
	} else if (strcmp(str, "source-out") == 0) {
		op = CAIRO_OPERATOR_OUT;
	} else if (strcmp(str, "destination-out") == 0) {
		op = CAIRO_OPERATOR_DEST_OUT;
	} else if (strcmp(str, "source-atop") == 0) {
		op = CAIRO_OPERATOR_ATOP;
	} else if (strcmp(str, "destination-atop") == 0) {
		op = CAIRO_OPERATOR_DEST_ATOP;
	} else if (strcmp(str, "xor") == 0) {
		op = CAIRO_OPERATOR_XOR;
	} else if (strcmp(str, "lighter") == 0) {
		op = CAIRO_OPERATOR_ADD;
	} else if (strcmp(str, "normal") == 0) {
		// blend modes:
		op = CAIRO_OPERATOR_OVER;
	} else if (strcmp(str, "multiply") == 0) {
		op = CAIRO_OPERATOR_MULTIPLY;
	} else if (strcmp(str, "screen") == 0) {
		op = CAIRO_OPERATOR_SCREEN;
	} else if (strcmp(str, "overlay") == 0) {
		op = CAIRO_OPERATOR_OVERLAY;
	} else if (strcmp(str, "darken") == 0) {
		op = CAIRO_OPERATOR_DARKEN;
	} else if (strcmp(str, "lighten") == 0) {
		op = CAIRO_OPERATOR_LIGHTEN;
	} else if (strcmp(str, "color-dodge") == 0) {
		op = CAIRO_OPERATOR_COLOR_DODGE;
	} else if (strcmp(str, "color-burn") == 0) {
		op = CAIRO_OPERATOR_COLOR_BURN;
	} else if (strcmp(str, "hard-light") == 0) {
		op = CAIRO_OPERATOR_HARD_LIGHT;
	} else if (strcmp(str, "soft-light") == 0) {
		op = CAIRO_OPERATOR_SOFT_LIGHT;
	} else if (strcmp(str, "difference") == 0) {
		op = CAIRO_OPERATOR_DIFFERENCE;
	} else if (strcmp(str, "exclusion") == 0) {
		op = CAIRO_OPERATOR_EXCLUSION;
	} else if (strcmp(str, "hue") == 0) {
		op = CAIRO_OPERATOR_HSL_HUE;
	} else if (strcmp(str, "saturation") == 0) {
		op = CAIRO_OPERATOR_HSL_SATURATION;
	} else if (strcmp(str, "color") == 0) {
		op = CAIRO_OPERATOR_HSL_COLOR;
	} else if (strcmp(str, "luminosity") == 0) {
		op = CAIRO_OPERATOR_HSL_LUMINOSITY;
	} else if (strcmp(str, "saturate") == 0) {
		// non-standard:
		op = CAIRO_OPERATOR_SATURATE;
	}
	JS_FreeCString(ctx, str);
	if (op != -1) {
		cairo_set_operator(cr, op);
	}
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_get_image_smoothing_enabled(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	return JS_NewBool(ctx, context->state->image_smoothing_enabled);
}

static JSValue nx_canvas_context_2d_set_image_smoothing_enabled(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	int value = JS_ToBool(ctx, argv[0]);
	if (value == -1)
		return JS_EXCEPTION;
	context->state->image_smoothing_enabled = value;
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_get_image_smoothing_quality(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	const char *quality;
	switch (context->state->image_smoothing_quality) {
	case CAIRO_FILTER_BEST:
		quality = "high";
		break;
	case CAIRO_FILTER_GOOD:
		quality = "medium";
		break;
	default:
		quality = "low";
	}
	return JS_NewString(ctx, quality);
}

static JSValue nx_canvas_context_2d_set_image_smoothing_quality(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	const char *str = JS_ToCString(ctx, argv[0]);
	if (!str)
		return JS_EXCEPTION;
	if (strcmp(str, "high") == 0) {
		context->state->image_smoothing_quality = CAIRO_FILTER_BEST;
	} else if (strcmp(str, "medium") == 0) {
		context->state->image_smoothing_quality = CAIRO_FILTER_GOOD;
	} else if (strcmp(str, "low") == 0) {
		context->state->image_smoothing_quality = CAIRO_FILTER_FAST;
	}
	JS_FreeCString(ctx, str);
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_get_image_data(JSContext *ctx,
												   JSValueConst this_val,
												   int argc,
												   JSValueConst *argv) {
	CANVAS_CONTEXT_ARGV0;
	uint32_t width = context->canvas->width;
	uint32_t height = context->canvas->height;

	int sx;
	int sy;
	int sw;
	int sh;
	if (JS_ToInt32(ctx, &sx, argv[1]) || JS_ToInt32(ctx, &sy, argv[2]) ||
		JS_ToInt32(ctx, &sw, argv[3]) || JS_ToInt32(ctx, &sh, argv[4])) {
		return JS_EXCEPTION;
	}

	// WebKit and Firefox have this behavior:
	// Flip the coordinates so the origin is top/left-most:
	if (sw < 0) {
		sx += sw;
		sw = -sw;
	}
	if (sh < 0) {
		sy += sh;
		sh = -sh;
	}

	if (sx + sw > width)
		sw = width - sx;
	if (sy + sh > height)
		sh = height - sy;

	// WebKit/moz functionality
	if (sw <= 0)
		sw = 1;
	if (sh <= 0)
		sh = 1;

	// Non-compliant. "Pixels outside the canvas must be returned as transparent
	// black." This instead clips the returned array to the canvas area.
	if (sx < 0) {
		sw += sx;
		sx = 0;
	}
	if (sy < 0) {
		sh += sy;
		sy = 0;
	}

	int srcStride = width * 4;
	int bpp = srcStride / width;
	size_t size = sw * sh * bpp;
	int dstStride = sw * bpp;

	uint8_t *src = context->canvas->data;

	uint8_t *dst = js_malloc(ctx, size);
	if (!dst) {
		JS_ThrowOutOfMemory(ctx);
		return JS_EXCEPTION;
	}

	JSValue ab =
		JS_NewArrayBuffer(ctx, dst, size, js_free_array_buffer, NULL, 0);

	if (JS_IsException(ab)) {
		js_free(ctx, dst); // Free data in case of exception
		return ab;
	}

	// Rearrange alpha (argb -> rgba), undo alpha pre-multiplication,
	// and store in big-endian format
	for (int y = 0; y < sh; ++y) {
		uint32_t *row = (uint32_t *)(src + srcStride * (y + sy));
		for (int x = 0; x < sw; ++x) {
			int bx = x * 4;
			uint32_t *pixel = row + x + sx;
			uint8_t a = *pixel >> 24;
			uint8_t r = *pixel >> 16;
			uint8_t g = *pixel >> 8;
			uint8_t b = *pixel;
			dst[bx + 3] = a;

			// Performance optimization: fully transparent/opaque pixels can be
			// processed more efficiently.
			if (a == 0 || a == 255) {
				dst[bx + 0] = r;
				dst[bx + 1] = g;
				dst[bx + 2] = b;
			} else {
				// Undo alpha pre-multiplication
				float alphaR = (float)255 / a;
				dst[bx + 0] = (int)((float)r * alphaR);
				dst[bx + 1] = (int)((float)g * alphaR);
				dst[bx + 2] = (int)((float)b * alphaR);
			}
		}
		dst += dstStride;
	}

	return ab;
}

static JSValue nx_canvas_context_2d_get_miter_limit(JSContext *ctx,
													JSValueConst this_val,
													int argc,
													JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	return JS_NewFloat64(ctx, cairo_get_miter_limit(cr));
}

static JSValue nx_canvas_context_2d_set_miter_limit(JSContext *ctx,
													JSValueConst this_val,
													int argc,
													JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	double limit;
	if (JS_ToFloat64(ctx, &limit, argv[0]))
		return JS_EXCEPTION;
	if (limit > 0) {
		cairo_set_miter_limit(cr, limit);
	}
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_get_text_align(JSContext *ctx,
												   JSValueConst this_val,
												   int argc,
												   JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	const char *align;
	switch (context->state->text_align) {
	default:
	case TEXT_ALIGN_START:
		align = "start";
		break;
	case TEXT_ALIGN_LEFT:
		align = "left";
		break;
	case TEXT_ALIGN_CENTER:
		align = "center";
		break;
	case TEXT_ALIGN_RIGHT:
		align = "right";
		break;
	case TEXT_ALIGN_END:
		align = "end";
		break;
	}
	return JS_NewString(ctx, align);
}

static JSValue nx_canvas_context_2d_set_text_align(JSContext *ctx,
												   JSValueConst this_val,
												   int argc,
												   JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	const char *str = JS_ToCString(ctx, argv[0]);
	if (!str)
		return JS_EXCEPTION;
	if (strcmp(str, "start") == 0) {
		context->state->text_align = TEXT_ALIGN_START;
	} else if (strcmp(str, "left") == 0) {
		context->state->text_align = TEXT_ALIGN_LEFT;
	} else if (strcmp(str, "center") == 0) {
		context->state->text_align = TEXT_ALIGN_CENTER;
	} else if (strcmp(str, "right") == 0) {
		context->state->text_align = TEXT_ALIGN_RIGHT;
	} else if (strcmp(str, "end") == 0) {
		context->state->text_align = TEXT_ALIGN_END;
	}
	JS_FreeCString(ctx, str);
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_get_text_baseline(JSContext *ctx,
													  JSValueConst this_val,
													  int argc,
													  JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	const char *v;
	switch (context->state->text_baseline) {
	case TEXT_BASELINE_TOP:
		v = "top";
		break;
	case TEXT_BASELINE_BOTTOM:
		v = "bottom";
		break;
	case TEXT_BASELINE_MIDDLE:
		v = "middle";
		break;
	case TEXT_BASELINE_IDEOGRAPHIC:
		v = "ideographic";
		break;
	case TEXT_BASELINE_HANGING:
		v = "hanging";
		break;
	default:
		v = "alphabetic";
		break;
	}
	return JS_NewString(ctx, v);
}

static JSValue nx_canvas_context_2d_set_text_baseline(JSContext *ctx,
													  JSValueConst this_val,
													  int argc,
													  JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	const char *str = JS_ToCString(ctx, argv[0]);
	if (!str)
		return JS_EXCEPTION;
	if (strcmp(str, "alphabetic") == 0) {
		context->state->text_baseline = TEXT_BASELINE_ALPHABETIC;
	} else if (strcmp(str, "top") == 0) {
		context->state->text_baseline = TEXT_BASELINE_TOP;
	} else if (strcmp(str, "middle") == 0) {
		context->state->text_baseline = TEXT_BASELINE_MIDDLE;
	} else if (strcmp(str, "bottom") == 0) {
		context->state->text_baseline = TEXT_BASELINE_BOTTOM;
	} else if (strcmp(str, "ideographic") == 0) {
		context->state->text_baseline = TEXT_BASELINE_IDEOGRAPHIC;
	} else if (strcmp(str, "hanging") == 0) {
		context->state->text_baseline = TEXT_BASELINE_HANGING;
	}
	JS_FreeCString(ctx, str);
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_rotate(JSContext *ctx,
										   JSValueConst this_val, int argc,
										   JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	double n;
	if (JS_ToFloat64(ctx, &n, argv[0]))
		return JS_EXCEPTION;
	cairo_rotate(cr, n);
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_scale(JSContext *ctx, JSValueConst this_val,
										  int argc, JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	double args[2];
	if (js_validate_doubles_args(ctx, argv, args, 2, 0))
		return JS_EXCEPTION;
	cairo_scale(cr, args[0], args[1]);
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_transform(JSContext *ctx,
											  JSValueConst this_val, int argc,
											  JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	double args[6];
	if (js_validate_doubles_args(ctx, argv, args, 6, 0))
		return JS_EXCEPTION;
	cairo_matrix_t matrix;
	cairo_matrix_init(&matrix, args[0], args[1], args[2], args[3], args[4],
					  args[5]);
	cairo_transform(cr, &matrix);
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_set_transform(JSContext *ctx,
												  JSValueConst this_val,
												  int argc,
												  JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	if (argc == 1 && JS_IsObject(argv[0])) {
		nx_dommatrix_t *dommatrix = nx_get_dommatrix(ctx, argv[0]);
		if (dommatrix) {
			// Happy case - the passed in value is a `DOMMatrix` instance
			cairo_set_matrix(cr, &dommatrix->cr_matrix);
		} else {
			// The passed in value is a plain JS object,
			// so construct the cairo matrix manually
			nx_dommatrix_t m;
			if (nx_dommatrix_init(ctx, argv[0], &m)) {
				return JS_EXCEPTION;
			}
			cairo_set_matrix(cr, &m.cr_matrix);
		}
	} else if (argc == 6) {
		cairo_matrix_t m;
		if (JS_ToFloat64(ctx, &m.xx, argv[0]) ||
			JS_ToFloat64(ctx, &m.yx, argv[1]) ||
			JS_ToFloat64(ctx, &m.xy, argv[2]) ||
			JS_ToFloat64(ctx, &m.yy, argv[3]) ||
			JS_ToFloat64(ctx, &m.x0, argv[4]) ||
			JS_ToFloat64(ctx, &m.y0, argv[5])) {
			return JS_EXCEPTION;
		}
		cairo_set_matrix(cr, &m);
	} else {
		cairo_identity_matrix(cr);
		return nx_canvas_context_2d_transform(ctx, this_val, argc, argv);
	}
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_reset_transform(JSContext *ctx,
													JSValueConst this_val,
													int argc,
													JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	cairo_identity_matrix(cr);
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_translate(JSContext *ctx,
											  JSValueConst this_val, int argc,
											  JSValueConst *argv) {
	CANVAS_CONTEXT_THIS;
	double args[2];
	if (js_validate_doubles_args(ctx, argv, args, 2, 0))
		return JS_EXCEPTION;
	cairo_translate(cr, args[0], args[1]);
	return JS_UNDEFINED;
}

/* CanvasGradient */

static void finalizer_canvas_gradient(JSRuntime *rt, JSValue val) {
	cairo_pattern_t *pattern =
		JS_GetOpaque(val, nx_canvas_gradient_class_id);
	if (pattern) {
		cairo_pattern_destroy(pattern);
	}
}

static JSValue nx_canvas_gradient_new_linear(JSContext *ctx,
											 JSValueConst this_val, int argc,
											 JSValueConst *argv) {
	double args[4];
	if (js_validate_doubles_args(ctx, argv, args, 4, 0))
		return JS_EXCEPTION;
	cairo_pattern_t *pattern =
		cairo_pattern_create_linear(args[0], args[1], args[2], args[3]);
	JSValue obj = JS_NewObjectClass(ctx, nx_canvas_gradient_class_id);
	if (JS_IsException(obj)) {
		cairo_pattern_destroy(pattern);
		return obj;
	}
	JS_SetOpaque(obj, pattern);
	return obj;
}

static JSValue nx_canvas_gradient_new_radial(JSContext *ctx,
											 JSValueConst this_val, int argc,
											 JSValueConst *argv) {
	double args[6];
	if (js_validate_doubles_args(ctx, argv, args, 6, 0))
		return JS_EXCEPTION;
	cairo_pattern_t *pattern = cairo_pattern_create_radial(
		args[0], args[1], args[2], args[3], args[4], args[5]);
	JSValue obj = JS_NewObjectClass(ctx, nx_canvas_gradient_class_id);
	if (JS_IsException(obj)) {
		cairo_pattern_destroy(pattern);
		return obj;
	}
	JS_SetOpaque(obj, pattern);
	return obj;
}

static JSValue nx_canvas_gradient_add_color_stop_standalone(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	cairo_pattern_t *pattern =
		JS_GetOpaque2(ctx, argv[0], nx_canvas_gradient_class_id);
	if (!pattern)
		return JS_EXCEPTION;
	double args[5];
	if (js_validate_doubles_args(ctx, argv, args, 5, 1))
		return JS_EXCEPTION;
	cairo_pattern_add_color_stop_rgba(pattern, args[0], args[1] / 255.,
									  args[2] / 255., args[3] / 255., args[4]);
	return JS_UNDEFINED;
}

static JSValue nx_canvas_gradient_init_class(JSContext *ctx,
											 JSValueConst this_val, int argc,
											 JSValueConst *argv) {
	(void)ctx;
	(void)this_val;
	(void)argc;
	(void)argv;
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_set_fill_style_gradient(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	CANVAS_CONTEXT_ARGV0;
	cairo_pattern_t *pattern =
		JS_GetOpaque2(ctx, argv[1], nx_canvas_gradient_class_id);
	if (!pattern)
		return JS_EXCEPTION;
	if (context->state->fill_gradient) {
		cairo_pattern_destroy(context->state->fill_gradient);
	}
	cairo_pattern_reference(pattern);
	context->state->fill_gradient = pattern;
	context->state->fill_source_type = SOURCE_GRADIENT;
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_set_stroke_style_gradient(
	JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	CANVAS_CONTEXT_ARGV0;
	cairo_pattern_t *pattern =
		JS_GetOpaque2(ctx, argv[1], nx_canvas_gradient_class_id);
	if (!pattern)
		return JS_EXCEPTION;
	if (context->state->stroke_gradient) {
		cairo_pattern_destroy(context->state->stroke_gradient);
	}
	cairo_pattern_reference(pattern);
	context->state->stroke_gradient = pattern;
	context->state->stroke_source_type = SOURCE_GRADIENT;
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_init_class(JSContext *ctx,
											   JSValueConst this_val, int argc,
											   JSValueConst *argv) {
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_GETSET(proto, "globalAlpha", nx_canvas_context_2d_get_global_alpha,
				  nx_canvas_context_2d_set_global_alpha);
	NX_DEF_GETSET(proto, "globalCompositeOperation",
				  nx_canvas_context_2d_get_global_composite_operation,
				  nx_canvas_context_2d_set_global_composite_operation);
	NX_DEF_GETSET(proto, "imageSmoothingEnabled",
				  nx_canvas_context_2d_get_image_smoothing_enabled,
				  nx_canvas_context_2d_set_image_smoothing_enabled);
	NX_DEF_GETSET(proto, "imageSmoothingQuality",
				  nx_canvas_context_2d_get_image_smoothing_quality,
				  nx_canvas_context_2d_set_image_smoothing_quality);
	NX_DEF_GETSET(proto, "lineCap", nx_canvas_context_2d_get_line_cap,
				  nx_canvas_context_2d_set_line_cap);
	NX_DEF_GETSET(proto, "lineDashOffset",
				  nx_canvas_context_2d_get_line_dash_offset,
				  nx_canvas_context_2d_set_line_dash_offset);
	NX_DEF_GETSET(proto, "lineJoin", nx_canvas_context_2d_get_line_join,
				  nx_canvas_context_2d_set_line_join);
	NX_DEF_GETSET(proto, "lineWidth", nx_canvas_context_2d_get_line_width,
				  nx_canvas_context_2d_set_line_width);
	NX_DEF_GETSET(proto, "miterLimit", nx_canvas_context_2d_get_miter_limit,
				  nx_canvas_context_2d_set_miter_limit);
	NX_DEF_GETSET(proto, "textAlign", nx_canvas_context_2d_get_text_align,
				  nx_canvas_context_2d_set_text_align);
	NX_DEF_GETSET(proto, "textBaseline", nx_canvas_context_2d_get_text_baseline,
				  nx_canvas_context_2d_set_text_baseline);
	NX_DEF_FUNC(proto, "arc", nx_canvas_context_2d_arc, 5);
	NX_DEF_FUNC(proto, "arcTo", nx_canvas_context_2d_arc_to, 5);
	NX_DEF_FUNC(proto, "beginPath", nx_canvas_context_2d_begin_path, 0);
	NX_DEF_FUNC(proto, "bezierCurveTo", nx_canvas_context_2d_bezier_curve_to,
				6);
	NX_DEF_FUNC(proto, "clearRect", nx_canvas_context_2d_clear_rect, 4);
	NX_DEF_FUNC(proto, "closePath", nx_canvas_context_2d_close_path, 0);
	NX_DEF_FUNC(proto, "clip", nx_canvas_context_2d_clip, 0);
	NX_DEF_FUNC(proto, "drawImage", nx_canvas_context_2d_draw_image, 3);
	NX_DEF_FUNC(proto, "ellipse", nx_canvas_context_2d_ellipse, 7);
	NX_DEF_FUNC(proto, "fill", nx_canvas_context_2d_fill, 0);
	NX_DEF_FUNC(proto, "fillRect", nx_canvas_context_2d_fill_rect, 4);
	NX_DEF_FUNC(proto, "fillText", nx_canvas_context_2d_fill_text, 3);
	NX_DEF_FUNC(proto, "getLineDash", nx_canvas_context_2d_get_line_dash, 0);
	NX_DEF_FUNC(proto, "isPointInPath", nx_canvas_context_2d_is_point_in_path,
				2);
	NX_DEF_FUNC(proto, "isPointInStroke",
				nx_canvas_context_2d_is_point_in_stroke, 2);
	NX_DEF_FUNC(proto, "lineTo", nx_canvas_context_2d_line_to, 2);
	NX_DEF_FUNC(proto, "measureText", nx_canvas_context_2d_measure_text, 1);
	NX_DEF_FUNC(proto, "moveTo", nx_canvas_context_2d_move_to, 2);
	NX_DEF_FUNC(proto, "putImageData", nx_canvas_context_2d_put_image_data, 3);
	NX_DEF_FUNC(proto, "quadraticCurveTo",
				nx_canvas_context_2d_quadratic_curve_to, 4);
	NX_DEF_FUNC(proto, "rect", nx_canvas_context_2d_rect, 4);
	NX_DEF_FUNC(proto, "resetTransform", nx_canvas_context_2d_reset_transform,
				0);
	NX_DEF_FUNC(proto, "restore", nx_canvas_context_2d_restore, 0);
	NX_DEF_FUNC(proto, "rotate", nx_canvas_context_2d_rotate, 1);
	NX_DEF_FUNC(proto, "roundRect", nx_canvas_context_2d_round_rect, 4);
	NX_DEF_FUNC(proto, "save", nx_canvas_context_2d_save, 0);
	NX_DEF_FUNC(proto, "scale", nx_canvas_context_2d_scale, 2);
	NX_DEF_FUNC(proto, "setLineDash", nx_canvas_context_2d_set_line_dash, 1);
	NX_DEF_FUNC(proto, "setTransform", nx_canvas_context_2d_set_transform, 0);
	NX_DEF_FUNC(proto, "stroke", nx_canvas_context_2d_stroke, 0);
	NX_DEF_FUNC(proto, "strokeRect", nx_canvas_context_2d_stroke_rect, 4);
	NX_DEF_FUNC(proto, "strokeText", nx_canvas_context_2d_stroke_text, 3);
	NX_DEF_FUNC(proto, "transform", nx_canvas_context_2d_transform, 6);
	NX_DEF_FUNC(proto, "translate", nx_canvas_context_2d_translate, 2);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static JSValue nx_canvas_context_2d_new(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	nx_canvas_t *canvas = nx_get_canvas(ctx, argv[0]);

	nx_canvas_context_2d_t *context =
		js_mallocz(ctx, sizeof(nx_canvas_context_2d_t));
	nx_canvas_context_2d_state_t *state =
		js_mallocz(ctx, sizeof(nx_canvas_context_2d_state_t));
	if (!context || !state)
		return JS_EXCEPTION;

	JSValue obj = JS_NewObjectClass(ctx, nx_canvas_context_class_id);
	if (JS_IsException(obj)) {
		js_free(ctx, context);
		js_free(ctx, state);
		return obj;
	}

	context->canvas = canvas;
	context->state = state;
	context->ctx = cairo_create(canvas->surface);

	// Match browser defaults
	state->next = NULL;
	state->font = JS_UNDEFINED;
	state->font_size = 10.;
	state->fill.a = 1.;
	state->stroke.a = 1.;
	state->fill_source_type = SOURCE_RGBA;
	state->stroke_source_type = SOURCE_RGBA;
	state->fill_gradient = NULL;
	state->stroke_gradient = NULL;
	state->global_alpha = 1.;
	state->image_smoothing_quality = CAIRO_FILTER_FAST;
	state->image_smoothing_enabled = true;
	state->text_align = TEXT_ALIGN_START;
	state->text_baseline = TEXT_BASELINE_ALPHABETIC;
	cairo_set_line_width(context->ctx, 1.);

	JS_SetOpaque(obj, context);
	return obj;
}

static void finalizer_canvas(JSRuntime *rt, JSValue val) {
	nx_canvas_t *context = JS_GetOpaque(val, nx_canvas_class_id);
	if (context) {
		if (context->surface) {
			cairo_surface_destroy(context->surface);
		}
		if (context->data) {
			js_free_rt(rt, context->data);
		}
		js_free_rt(rt, context);
	}
}

static const JSCFunctionListEntry init_function_list[] = {
	JS_CFUNC_DEF("canvasNew", 0, nx_canvas_new),
	JS_CFUNC_DEF("canvasInitClass", 0, nx_canvas_init_class),
	JS_CFUNC_DEF("canvasContext2dNew", 0, nx_canvas_context_2d_new),
	JS_CFUNC_DEF("canvasContext2dInitClass", 0,
				 nx_canvas_context_2d_init_class),
	JS_CFUNC_DEF("canvasContext2dGetImageData", 0,
				 nx_canvas_context_2d_get_image_data),
	JS_CFUNC_DEF("canvasContext2dGetTransform", 0,
				 nx_canvas_context_2d_get_transform),
	JS_CFUNC_DEF("canvasContext2dGetFont", 0, nx_canvas_context_2d_get_font),
	JS_CFUNC_DEF("canvasContext2dSetFont", 0, nx_canvas_context_2d_set_font),
	JS_CFUNC_DEF("canvasContext2dGetFillStyle", 0,
				 nx_canvas_context_2d_get_fill_style),
	JS_CFUNC_DEF("canvasContext2dSetFillStyle", 0,
				 nx_canvas_context_2d_set_fill_style),
	JS_CFUNC_DEF("canvasContext2dGetStrokeStyle", 0,
				 nx_canvas_context_2d_get_stroke_style),
	JS_CFUNC_DEF("canvasContext2dSetStrokeStyle", 0,
				 nx_canvas_context_2d_set_stroke_style),
	JS_CFUNC_DEF("canvasContext2dSetFillStyleGradient", 0,
				 nx_canvas_context_2d_set_fill_style_gradient),
	JS_CFUNC_DEF("canvasContext2dSetStrokeStyleGradient", 0,
				 nx_canvas_context_2d_set_stroke_style_gradient),
	JS_CFUNC_DEF("canvasGradientNewLinear", 0, nx_canvas_gradient_new_linear),
	JS_CFUNC_DEF("canvasGradientNewRadial", 0, nx_canvas_gradient_new_radial),
	JS_CFUNC_DEF("canvasGradientInitClass", 0, nx_canvas_gradient_init_class),
	JS_CFUNC_DEF("canvasGradientAddColorStop", 0,
				 nx_canvas_gradient_add_color_stop_standalone),
};

void nx_init_canvas(JSContext *ctx, JSValueConst init_obj) {
	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_canvas_class_id);
	JSClassDef canvas_class = {
		"nx_canvas_t",
		.finalizer = finalizer_canvas,
	};
	JS_NewClass(rt, nx_canvas_class_id, &canvas_class);

	JS_NewClassID(rt, &nx_canvas_context_class_id);
	JSClassDef canvas_context_2d_class = {
		"nx_canvas_context_2d_t",
		.finalizer = finalizer_canvas_context_2d,
	};
	JS_NewClass(rt, nx_canvas_context_class_id, &canvas_context_2d_class);

	JS_NewClassID(rt, &nx_canvas_gradient_class_id);
	JSClassDef canvas_gradient_class = {
		"nx_canvas_gradient_t",
		.finalizer = finalizer_canvas_gradient,
	};
	JS_NewClass(rt, nx_canvas_gradient_class_id, &canvas_gradient_class);

	JS_SetPropertyFunctionList(ctx, init_obj, init_function_list,
							   countof(init_function_list));
}
