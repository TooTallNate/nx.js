/**
 * Shared canvas path-geometry builders (LOCAL / user coordinates).
 *
 * Implementation moved verbatim from source/canvas.cc so it can be reused by
 * both the canvas 2D context and Path2D. No v8/JS dependency and no CTM
 * handling — these build LOCAL-space sub-paths only.
 */
#include "canvas_path.h"

#include <optional>

#include "include/core/SkRect.h"

namespace {

static const double kTwoPi = M_PI * 2.;

typedef struct Point {
	float x, y;
} Point;

static inline float minf(float a, float b) { return a < b ? a : b; }

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

}  // namespace

// HTML-canvas arc angle canonicalization (ported from the Cairo path). Maps
// (startAngle, endAngle, counterclockwise) to a start angle in [0,2pi) and an
// adjusted end angle whose signed difference is the exact sweep to draw —
// crucially yielding +/-2pi (a full circle) for arc(.., 0, 2*PI, ..) rather
// than 0.
void canonicalizeAngle(double *startAngle, double *endAngle) {
	double newStartAngle = fmod(*startAngle, kTwoPi);
	if (newStartAngle < 0) {
		newStartAngle += kTwoPi;
		if (newStartAngle >= kTwoPi)
			newStartAngle -= kTwoPi;
	}
	double delta = newStartAngle - *startAngle;
	*startAngle = newStartAngle;
	*endAngle = *endAngle + delta;
}
double adjustEndAngle(double startAngle, double endAngle,
                      bool counterclockwise) {
	double newEndAngle = endAngle;
	if (!counterclockwise && endAngle - startAngle >= kTwoPi)
		newEndAngle = startAngle + kTwoPi;
	else if (counterclockwise && startAngle - endAngle >= kTwoPi)
		newEndAngle = startAngle - kTwoPi;
	else if (!counterclockwise && startAngle > endAngle)
		newEndAngle =
		    startAngle + (kTwoPi - fmod(startAngle - endAngle, kTwoPi));
	else if (counterclockwise && startAngle < endAngle)
		newEndAngle =
		    startAngle - (kTwoPi - fmod(endAngle - startAngle, kTwoPi));
	return newEndAngle;
}

// Append a circular arc to `path`, matching cairo_arc / cairo_arc_negative:
// the arc is connected to the current point with a line (or starts the
// contour). `ccw` selects the negative (counter-clockwise) direction.
// Append an arc from angle a1 to a2. The caller is responsible for choosing a1
// and a2 such that the directed span (a2 - a1) is the EXACT signed sweep to
// draw (clockwise if a2>a1, counter-clockwise if a2<a1), including a full
// +/-2pi for a complete circle. This mirrors cairo_arc / cairo_arc_negative,
// which draw the literal angular span. (The HTML-canvas-spec angle
// canonicalization is done in nx_canvas_context_2d_arc before calling here.)
void path_arc(SkPathBuilder &path, double xc, double yc, double radius,
              double a1, double a2, bool ccw) {
	// `ccw` only documents intent; the actual direction is encoded in a2 - a1.
	(void)ccw;
	double sweep = a2 - a1;
	double startX = xc + radius * cos(a1);
	double startY = yc + radius * sin(a1);
	if (path_has_current(path, nullptr))
		path.lineTo((SkScalar)startX, (SkScalar)startY);
	else
		path.moveTo((SkScalar)startX, (SkScalar)startY);
	SkRect oval = SkRect::MakeLTRB((SkScalar)(xc - radius),
	                               (SkScalar)(yc - radius),
	                               (SkScalar)(xc + radius),
	                               (SkScalar)(yc + radius));
	double startDeg = a1 * 180.0 / M_PI;
	double sweepDeg = sweep * 180.0 / M_PI;
	// SkPath::arcTo cannot draw a sweep of magnitude >= 360 in one call (it is
	// documented as "always less than 360 degrees"), so a full circle would be
	// dropped. Split into <=180-degree chunks; forceMoveTo=false keeps the
	// chunks connected and connected to the prior current point.
	double remaining = sweepDeg;
	double cur = startDeg;
	while (fabs(remaining) > 1e-6) {
		double step = remaining;
		if (step > 180.0)
			step = 180.0;
		else if (step < -180.0)
			step = -180.0;
		path.arcTo(oval, (SkScalar)cur, (SkScalar)step, false);
		cur += step;
		remaining -= step;
	}
}

// ---------------------------------------------------------------------------
// High-level primitives (LOCAL coordinates)
// ---------------------------------------------------------------------------

void nxcp_arc(SkPathBuilder &sub, double x, double y, double r,
              double startAngle, double endAngle, bool ccw) {
	canonicalizeAngle(&startAngle, &endAngle);
	endAngle = adjustEndAngle(startAngle, endAngle, ccw);
	path_arc(sub, x, y, r, startAngle, endAngle, ccw);
}

void nxcp_ellipse(SkPathBuilder &sub, double x, double y, double rx, double ry,
                  double rotation, double startAngle, double endAngle, bool ccw,
                  bool has_current, SkPoint p0_local) {
	// Build the arc on the ellipse by transforming a true unit-circle arc
	// (radius 1, centered at the origin) by translate(x,y)*rotate*scale(rx,ry),
	// matching the Cairo scale-trick. The unit arc must
	// use radius 1 at the origin so the scale maps it to radii (rx,ry); using
	// a non-unit radius or off-origin center double-applies the scale and
	// collapses/offsets the ellipse (leaving the interior unfilled).
	//
	// Ellipse-space -> LOCAL-space matrix.
	SkMatrix m = SkMatrix::Translate((SkScalar)x, (SkScalar)y);
	m.preRotate((SkScalar)(rotation * 180.0 / M_PI));
	m.preScale((SkScalar)rx, (SkScalar)ry);
	SkPathBuilder unit;
	if (has_current) {
		// Seed with the current (LOCAL-space) point mapped back into ellipse
		// space so the connecting line is correct after transform.
		SkMatrix inv;
		if (m.invert(&inv))
			unit.moveTo(inv.mapPoint(p0_local));
	}
	// Canonicalize like arc() so a full-circle ellipse (0..2pi) is preserved.
	canonicalizeAngle(&startAngle, &endAngle);
	endAngle = adjustEndAngle(startAngle, endAngle, ccw);
	path_arc(unit, 0., 0., 1., startAngle, endAngle, ccw);
	SkPath unit_path = unit.detach();
	sub.addPath(unit_path, m,
	            has_current ? SkPath::kExtend_AddPathMode
	                        : SkPath::kAppend_AddPathMode);
}

void nxcp_arc_to(SkPathBuilder &sub, SkPoint p0pt, double x1, double y1,
                 double x2, double y2, double radius_d) {
	Point p0 = {p0pt.x(), p0pt.y()};
	Point p1 = {(float)x1, (float)y1};
	Point p2 = {(float)x2, (float)y2};
	float radius = (float)radius_d;
	if ((p1.x == p0.x && p1.y == p0.y) || (p1.x == p2.x && p1.y == p2.y) ||
	    radius == 0.f) {
		sub.lineTo(p1.x, p1.y);
		return;
	}
	Point p1p0 = {(p0.x - p1.x), (p0.y - p1.y)};
	Point p1p2 = {(p2.x - p1.x), (p2.y - p1.y)};
	float p1p0_length = sqrtf(p1p0.x * p1p0.x + p1p0.y * p1p0.y);
	float p1p2_length = sqrtf(p1p2.x * p1p2.x + p1p2.y * p1p2.y);
	double cos_phi =
	    (p1p0.x * p1p2.x + p1p0.y * p1p2.y) / (p1p0_length * p1p2_length);
	if (-1 == cos_phi) {
		sub.lineTo(p1.x, p1.y);
		return;
	}
	if (1 == cos_phi) {
		unsigned int max_length = 65535;
		double factor_max = max_length / p1p0_length;
		Point ep = {(float)(p0.x + factor_max * p1p0.x),
		            (float)(p0.y + factor_max * p1p0.y)};
		sub.lineTo(ep.x, ep.y);
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
	sub.lineTo(t_p1p0.x, t_p1p0.y);
	path_arc(sub, p.x, p.y, radius, sa, ea, anticlockwise != 0);
}

void nxcp_rect(SkPathBuilder &sub, double x, double y, double width,
               double height) {
	// rect() always starts a new subpath, so the caller appends (don't extend).
	if (width == 0) {
		sub.moveTo((SkScalar)x, (SkScalar)y);
		sub.lineTo((SkScalar)x, (SkScalar)(y + height));
	} else if (height == 0) {
		sub.moveTo((SkScalar)x, (SkScalar)y);
		sub.lineTo((SkScalar)(x + width), (SkScalar)y);
	} else {
		// CSS rect() starts a new subpath: addRect moves to the corner and
		// closes, matching cairo_rectangle.
		sub.addRect(SkRect::MakeXYWH((SkScalar)x, (SkScalar)y,
		                             (SkScalar)width, (SkScalar)height));
	}
}

void nxcp_round_rect(SkPathBuilder &sub, double x, double y, double width,
                     double height, const SkVector radii_in[4],
                     bool clockwise) {
	// radii_in are in SkRRect order (TL, TR, BR, BL) after negative-dimension
	// corner swaps. Apply the spec radii down-scale (clamp), matching the
	// Cairo path: top/right/bottom/left sums are clamped to the box extents.
	Point upperLeft = {radii_in[0].x(), radii_in[0].y()};
	Point upperRight = {radii_in[1].x(), radii_in[1].y()};
	Point lowerRight = {radii_in[2].x(), radii_in[2].y()};
	Point lowerLeft = {radii_in[3].x(), radii_in[3].y()};
	{
		float top = upperLeft.x + upperRight.x;
		float right = upperRight.y + lowerRight.y;
		float bottom = lowerRight.x + lowerLeft.x;
		float left = upperLeft.y + lowerLeft.y;
		float scale = minf(
		    width / top, minf(height / right,
		                      minf(width / bottom, height / left)));
		if (scale < 1.) {
			upperLeft.x *= scale;
			upperLeft.y *= scale;
			upperRight.x *= scale;
			upperRight.y *= scale;
			lowerLeft.x *= scale;
			lowerLeft.y *= scale;
			lowerRight.x *= scale;
			lowerRight.y *= scale;
		}
	}
	// Build the rounded rectangle with Skia's native rounded-rect primitive,
	// which draws each (possibly elliptical, possibly per-corner) radius
	// correctly. Hand-rolling the four corner arcs is error-prone: the
	// quarter-sweep angles wrap around 0/2pi, and getting one wrong draws a
	// 3/4 arc backwards (which carved a circular wedge out of a corner).
	// SkRRect takes radii in the order TL, TR, BR, BL — matching the canvas
	// spec corners after the negative-width/height swaps above.
	SkVector radii[4] = {
	    {(SkScalar)upperLeft.x, (SkScalar)upperLeft.y},
	    {(SkScalar)upperRight.x, (SkScalar)upperRight.y},
	    {(SkScalar)lowerRight.x, (SkScalar)lowerRight.y},
	    {(SkScalar)lowerLeft.x, (SkScalar)lowerLeft.y},
	};
	SkRRect rrect;
	rrect.setRectRadii(
	    SkRect::MakeXYWH((SkScalar)x, (SkScalar)y, (SkScalar)width,
	                     (SkScalar)height),
	    radii);
	// Canvas roundRect starts the subpath at (x + topLeftRadius, y) and winds
	// clockwise (CW) by default; the negative-dimension handling flips
	// `clockwise`. SkPathDirection::kCW matches the canvas default.
	sub.addRRect(rrect, clockwise ? SkPathDirection::kCW
	                              : SkPathDirection::kCCW);
}
