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

nx_path2d_t *self(const FunctionCallbackInfo<Value> &info) {
	return nx::Unwrap<nx_path2d_t>(info[0]);
}

double arg(const FunctionCallbackInfo<Value> &info, int i, double def) {
	if (info.Length() > i && info[i]->IsNumber()) {
		double d;
		if (info[i]->NumberValue(info.GetIsolate()->GetCurrentContext()).To(&d))
			return d;
	}
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
		p->builder.moveTo((SkScalar)arg(info, 1, 0), (SkScalar)arg(info, 2, 0));
}

void nx_path2d_line_to(const FunctionCallbackInfo<Value> &info) {
	nx_path2d_t *p = self(info);
	if (!p)
		return;
	SkScalar x = (SkScalar)arg(info, 1, 0), y = (SkScalar)arg(info, 2, 0);
	if (has_current(p->builder, nullptr))
		p->builder.lineTo(x, y);
	else
		p->builder.moveTo(x, y);
}

void nx_path2d_bezier_curve_to(const FunctionCallbackInfo<Value> &info) {
	nx_path2d_t *p = self(info);
	if (!p)
		return;
	SkPoint c1 = {(SkScalar)arg(info, 1, 0), (SkScalar)arg(info, 2, 0)};
	SkPoint c2 = {(SkScalar)arg(info, 3, 0), (SkScalar)arg(info, 4, 0)};
	SkPoint end = {(SkScalar)arg(info, 5, 0), (SkScalar)arg(info, 6, 0)};
	if (!has_current(p->builder, nullptr))
		p->builder.moveTo(c1);
	p->builder.cubicTo(c1, c2, end);
}

void nx_path2d_quadratic_curve_to(const FunctionCallbackInfo<Value> &info) {
	nx_path2d_t *p = self(info);
	if (!p)
		return;
	SkPoint cp = {(SkScalar)arg(info, 1, 0), (SkScalar)arg(info, 2, 0)};
	SkPoint end = {(SkScalar)arg(info, 3, 0), (SkScalar)arg(info, 4, 0)};
	if (!has_current(p->builder, nullptr))
		p->builder.moveTo(cp);
	p->builder.quadTo(cp, end);
}

void nx_path2d_arc(const FunctionCallbackInfo<Value> &info) {
	nx_path2d_t *p = self(info);
	if (!p)
		return;
	double x = arg(info, 1, 0), y = arg(info, 2, 0), r = arg(info, 3, 0),
	       sa = arg(info, 4, 0), ea = arg(info, 5, 0);
	bool ccw = info.Length() > 6 ? info[6]->BooleanValue(info.GetIsolate())
	                             : false;
	if (r < 0)
		return;
	nxcp_arc(p->builder, x, y, r, sa, ea, ccw);
}

void nx_path2d_arc_to(const FunctionCallbackInfo<Value> &info) {
	nx_path2d_t *p = self(info);
	if (!p)
		return;
	SkPoint p0;
	if (!has_current(p->builder, &p0))
		p0 = SkPoint::Make(0, 0);
	nxcp_arc_to(p->builder, p0, arg(info, 1, 0), arg(info, 2, 0),
	            arg(info, 3, 0), arg(info, 4, 0), arg(info, 5, 0));
}

void nx_path2d_ellipse(const FunctionCallbackInfo<Value> &info) {
	nx_path2d_t *p = self(info);
	if (!p)
		return;
	double x = arg(info, 1, 0), y = arg(info, 2, 0), rx = arg(info, 3, 0),
	       ry = arg(info, 4, 0), rot = arg(info, 5, 0), sa = arg(info, 6, 0),
	       ea = arg(info, 7, 0);
	bool ccw = info.Length() > 8 ? info[8]->BooleanValue(info.GetIsolate())
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
	nxcp_rect(p->builder, arg(info, 1, 0), arg(info, 2, 0), arg(info, 3, 0),
	          arg(info, 4, 0));
}

void nx_path2d_round_rect(const FunctionCallbackInfo<Value> &info) {
	nx_path2d_t *p = self(info);
	if (!p)
		return;
	Isolate *iso = info.GetIsolate();
	double x = arg(info, 1, 0), y = arg(info, 2, 0), w = arg(info, 3, 0),
	       h = arg(info, 4, 0);
	// radii: number | [number|{x,y}, ...] (1..4). Resolve to 4 (x,y) corners.
	SkVector radii[4] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};
	if (!nx_resolve_round_rect_radii(iso, info.Length() > 5 ? info[5]
	                                                        : Local<Value>(),
	                                 radii)) {
		nx_throw(iso, "Invalid radii for roundRect");
		return;
	}
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

// path2dAddPath(dst, src, [domMatrix]) — append src to dst, optionally
// transformed by the DOMMatrix. Skia transforms ALL segment types correctly.
void nx_path2d_add_path(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_path2d_t *dst = nx::Unwrap<nx_path2d_t>(info[0]);
	const SkPath *src = nx_path2d_get(iso, info[1]);
	if (!dst || !src)
		return;
	SkMatrix m = SkMatrix::I();
	if (info.Length() > 2 && info[2]->IsObject()) {
		nx_dommatrix_t dm;
		if (nx_dommatrix_init(iso, info[2], &dm) == 0) {
			// DOMMatrix (a c e / b d f) -> SkMatrix row-major.
			m.setAll((SkScalar)dm.values.m11, (SkScalar)dm.values.m21,
			         (SkScalar)dm.values.m41, (SkScalar)dm.values.m12,
			         (SkScalar)dm.values.m22, (SkScalar)dm.values.m42, 0, 0, 1);
		}
	}
	dst->builder.addPath(*src, m, SkPath::kAppend_AddPathMode);
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
	NX_SET_FUNC(init_obj, "path2dNew", nx_path2d_new);
	NX_SET_FUNC(init_obj, "path2dMoveTo", nx_path2d_move_to);
	NX_SET_FUNC(init_obj, "path2dLineTo", nx_path2d_line_to);
	NX_SET_FUNC(init_obj, "path2dBezierCurveTo", nx_path2d_bezier_curve_to);
	NX_SET_FUNC(init_obj, "path2dQuadraticCurveTo",
	            nx_path2d_quadratic_curve_to);
	NX_SET_FUNC(init_obj, "path2dArc", nx_path2d_arc);
	NX_SET_FUNC(init_obj, "path2dArcTo", nx_path2d_arc_to);
	NX_SET_FUNC(init_obj, "path2dEllipse", nx_path2d_ellipse);
	NX_SET_FUNC(init_obj, "path2dRect", nx_path2d_rect);
	NX_SET_FUNC(init_obj, "path2dRoundRect", nx_path2d_round_rect);
	NX_SET_FUNC(init_obj, "path2dClosePath", nx_path2d_close_path);
	NX_SET_FUNC(init_obj, "path2dAddPath", nx_path2d_add_path);
}
