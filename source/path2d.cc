#include "path2d.h"
#include "canvas.h"
#include "canvas_path.h"
#include "dommatrix.h"
#include "error.h"
#include "wrap.h"

#include "include/core/SkMatrix.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkRRect.h"
#include "include/utils/SkParsePath.h"

#include <algorithm>
#include <math.h>
#include <optional>

using namespace v8;

// ---------------------------------------------------------------------------
// Path2D: an SkPath built in USER space. The canvas CTM is applied when the
// path is consumed (canvas.cc apply_path / fill / stroke / clip /
// isPointInPath), matching the spec: Path2D coordinates are not affected by
// the transform at construction time, only at use time.
// ---------------------------------------------------------------------------
struct nx_path2d_s {
	SkPathBuilder builder;
};

namespace {

void free_path2d(nx_path2d_t *p) { delete p; }

// Methods are installed on the Path2D prototype (see nx_path2d_init_class), so
// the receiver is `this` and args start at index 0.
nx_path2d_t *self(const FunctionCallbackInfo<Value> &info) {
	return nx::Unwrap<nx_path2d_t>(info.This());
}

// Coerce argument `i` to a number via ToNumber (matching canvas.cc's
// get_doubles and normal JS behavior, e.g. lineTo('10','20')). `def` is used
// only when the argument is absent.
double arg(const FunctionCallbackInfo<Value> &info, int i, double def) {
	if (info.Length() <= i)
		return def;
	double d;
	if (info[i]->NumberValue(info.GetIsolate()->GetCurrentContext()).To(&d))
		return d;
	return def;
}

bool has_current(const SkPathBuilder &b, SkPoint *out) {
	if (b.countPoints() == 0)
		return false;
	std::optional<SkPoint> lp = b.getLastPt();
	if (!lp)
		return false;
	if (out)
		*out = *lp;
	return true;
}

// path2dNew([pathOrString]) -> wrapped Path2D native object.
void nx_path2d_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_path2d_t *p = new nx_path2d_s();
	if (info.Length() > 0) {
		if (info[0]->IsString()) {
			// SVG path data — let Skia parse it (handles relative commands,
			// elliptical arcs, shorthand, etc.).
			String::Utf8Value d(iso, info[0]);
			if (*d) {
				SkPath parsed;
				if (SkParsePath::FromSVGString(*d, &parsed))
					p->builder.addPath(parsed);
			}
		} else if (info[0]->IsObject()) {
			// Copy-construct from another Path2D.
			const SkPath *src = nx_path2d_get(iso, info[0]);
			if (src)
				p->builder.addPath(*src);
		}
	}
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_path2d_t>(iso, obj, p, free_path2d);
	info.GetReturnValue().Set(obj);
}

void nx_path2d_move_to(const FunctionCallbackInfo<Value> &info) {
	nx_path2d_t *p = self(info);
	if (p)
		p->builder.moveTo((SkScalar)arg(info, 0, 0), (SkScalar)arg(info, 1, 0));
}

void nx_path2d_line_to(const FunctionCallbackInfo<Value> &info) {
	nx_path2d_t *p = self(info);
	if (!p)
		return;
	SkScalar x = (SkScalar)arg(info, 0, 0), y = (SkScalar)arg(info, 1, 0);
	if (has_current(p->builder, nullptr))
		p->builder.lineTo(x, y);
	else
		p->builder.moveTo(x, y);
}

void nx_path2d_bezier_curve_to(const FunctionCallbackInfo<Value> &info) {
	nx_path2d_t *p = self(info);
	if (!p)
		return;
	SkPoint c1 = {(SkScalar)arg(info, 0, 0), (SkScalar)arg(info, 1, 0)};
	SkPoint c2 = {(SkScalar)arg(info, 2, 0), (SkScalar)arg(info, 3, 0)};
	SkPoint end = {(SkScalar)arg(info, 4, 0), (SkScalar)arg(info, 5, 0)};
	if (!has_current(p->builder, nullptr))
		p->builder.moveTo(c1);
	p->builder.cubicTo(c1, c2, end);
}

void nx_path2d_quadratic_curve_to(const FunctionCallbackInfo<Value> &info) {
	nx_path2d_t *p = self(info);
	if (!p)
		return;
	SkPoint cp = {(SkScalar)arg(info, 0, 0), (SkScalar)arg(info, 1, 0)};
	SkPoint end = {(SkScalar)arg(info, 2, 0), (SkScalar)arg(info, 3, 0)};
	if (!has_current(p->builder, nullptr))
		p->builder.moveTo(cp);
	p->builder.quadTo(cp, end);
}

void nx_path2d_arc(const FunctionCallbackInfo<Value> &info) {
	nx_path2d_t *p = self(info);
	if (!p)
		return;
	double x = arg(info, 0, 0), y = arg(info, 1, 0), r = arg(info, 2, 0),
	       sa = arg(info, 3, 0), ea = arg(info, 4, 0);
	bool ccw = info.Length() > 5 ? info[5]->BooleanValue(info.GetIsolate())
	                             : false;
	if (r < 0) {
		// Match CanvasRenderingContext2D.arc().
		info.GetIsolate()->ThrowException(Exception::RangeError(
		    nx_str(info.GetIsolate(), "The radius provided is negative.")));
		return;
	}
	nxcp_arc(p->builder, x, y, r, sa, ea, ccw);
}

void nx_path2d_arc_to(const FunctionCallbackInfo<Value> &info) {
	nx_path2d_t *p = self(info);
	if (!p)
		return;
	SkPoint p0;
	if (!has_current(p->builder, &p0))
		p0 = SkPoint::Make(0, 0);
	nxcp_arc_to(p->builder, p0, arg(info, 0, 0), arg(info, 1, 0),
	            arg(info, 2, 0), arg(info, 3, 0), arg(info, 4, 0));
}

void nx_path2d_ellipse(const FunctionCallbackInfo<Value> &info) {
	nx_path2d_t *p = self(info);
	if (!p)
		return;
	double x = arg(info, 0, 0), y = arg(info, 1, 0), rx = arg(info, 2, 0),
	       ry = arg(info, 3, 0), rot = arg(info, 4, 0), sa = arg(info, 5, 0),
	       ea = arg(info, 6, 0);
	bool ccw = info.Length() > 7 ? info[7]->BooleanValue(info.GetIsolate())
	                             : false;
	if (rx == 0 || ry == 0)
		return;
	SkPoint cur;
	bool had = has_current(p->builder, &cur);
	nxcp_ellipse(p->builder, x, y, rx, ry, rot, sa, ea, ccw, had, cur);
}

void nx_path2d_rect(const FunctionCallbackInfo<Value> &info) {
	nx_path2d_t *p = self(info);
	if (!p)
		return;
	nxcp_rect(p->builder, arg(info, 0, 0), arg(info, 1, 0), arg(info, 2, 0),
	          arg(info, 3, 0));
}

void nx_path2d_round_rect(const FunctionCallbackInfo<Value> &info) {
	nx_path2d_t *p = self(info);
	if (!p)
		return;
	Isolate *iso = info.GetIsolate();
	double x = arg(info, 0, 0), y = arg(info, 1, 0), w = arg(info, 2, 0),
	       h = arg(info, 3, 0);
	// radii: number | [number|{x,y}, ...] (1..4). Resolve to 4 (x,y) corners.
	// The shared helper throws the specific RangeError/message on bad input.
	SkVector radii[4] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
	if (!nx_resolve_round_rect_radii(
	        iso, info.Length() > 4 ? info[4] : Local<Value>(), radii))
		return;
	bool clockwise = true;
	if (w < 0) {
		clockwise = !clockwise;
		x += w;
		w = -w;
		std::swap(radii[0], radii[1]);
		std::swap(radii[3], radii[2]);
	}
	if (h < 0) {
		clockwise = !clockwise;
		y += h;
		h = -h;
		std::swap(radii[0], radii[1]);
		std::swap(radii[3], radii[2]);
	}
	nxcp_round_rect(p->builder, x, y, w, h, radii, clockwise);
}

void nx_path2d_close_path(const FunctionCallbackInfo<Value> &info) {
	nx_path2d_t *p = self(info);
	if (p)
		p->builder.close();
}

// addPath(src, [domMatrix]) — append src to `this`, optionally transformed by
// the DOMMatrix. Skia transforms ALL segment types correctly.
void nx_path2d_add_path(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_path2d_t *dst = self(info);
	const SkPath *src = nx_path2d_get(iso, info[0]);
	if (!dst || !src)
		return;
	SkMatrix m = SkMatrix::I();
	if (info.Length() > 1 && info[1]->IsObject()) {
		// nx_dommatrix_init uses the struct's existing fields as defaults for
		// any missing DOMMatrixInit property, so start from identity.
		nx_dommatrix_t dm = {};
		dm.is_2d = true;
		dm.values.m11 = dm.values.m22 = dm.values.m33 = dm.values.m44 = 1.0;
		if (nx_dommatrix_init(iso, info[1], &dm) == 0) {
			// DOMMatrix (a c e / b d f) -> SkMatrix row-major.
			m.setAll((SkScalar)dm.values.m11, (SkScalar)dm.values.m21,
			         (SkScalar)dm.values.m41, (SkScalar)dm.values.m12,
			         (SkScalar)dm.values.m22, (SkScalar)dm.values.m42, 0, 0, 1);
		}
	}
	dst->builder.addPath(*src, m, SkPath::kAppend_AddPathMode);
}

// path2dInitClass(Path2D) — install the Path2D methods on the prototype. The
// TS class (canvas/path2d.ts) is just declarations + JSDoc with stub() bodies;
// the real implementations live here.
void nx_path2d_init_class(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	Local<Object> proto = info[0]
	                          .As<Object>()
	                          ->Get(ctx, nx_str(iso, "prototype"))
	                          .ToLocalChecked()
	                          .As<Object>();
	NX_DEF_FUNC(proto, "moveTo", nx_path2d_move_to, 2);
	NX_DEF_FUNC(proto, "lineTo", nx_path2d_line_to, 2);
	NX_DEF_FUNC(proto, "bezierCurveTo", nx_path2d_bezier_curve_to, 6);
	NX_DEF_FUNC(proto, "quadraticCurveTo", nx_path2d_quadratic_curve_to, 4);
	NX_DEF_FUNC(proto, "arc", nx_path2d_arc, 5);
	NX_DEF_FUNC(proto, "arcTo", nx_path2d_arc_to, 5);
	NX_DEF_FUNC(proto, "ellipse", nx_path2d_ellipse, 7);
	NX_DEF_FUNC(proto, "rect", nx_path2d_rect, 4);
	NX_DEF_FUNC(proto, "roundRect", nx_path2d_round_rect, 4);
	NX_DEF_FUNC(proto, "closePath", nx_path2d_close_path, 0);
	NX_DEF_FUNC(proto, "addPath", nx_path2d_add_path, 1);
}

} // namespace

const SkPath *nx_path2d_get(Isolate *iso, Local<Value> obj) {
	(void)iso;
	nx_path2d_t *p = nx::Unwrap<nx_path2d_t>(obj);
	if (!p)
		return nullptr;
	// snapshot() returns a stable SkPath view of the builder; cache it so the
	// returned pointer stays valid for the caller.
	static thread_local SkPath snap;
	snap = p->builder.snapshot();
	return &snap;
}

void nx_init_path2d(Isolate *iso, Local<Object> init_obj) {
	// Constructor backing (allocates the native SkPath) + prototype installer.
	NX_SET_FUNC(init_obj, "path2dNew", nx_path2d_new);
	NX_SET_FUNC(init_obj, "path2dInitClass", nx_path2d_init_class);
}
