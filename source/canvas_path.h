#pragma once

/**
 * Shared canvas path-geometry builders (LOCAL / user coordinates).
 *
 * These functions build canvas path primitives (arc, arcTo, ellipse, rect,
 * roundRect) into an SkPathBuilder in LOCAL coordinates, taking the current
 * point explicitly where relevant. They contain NO v8 / JS dependency and no
 * current-transformation-matrix (CTM) handling — callers in canvas.cc bake the
 * CTM into device space separately via add_user_subpath(), while Path2D reuses
 * the same math directly in user space.
 *
 * The math here is moved verbatim from source/canvas.cc; behavior is identical.
 */

#include <cmath>

#include "include/core/SkMatrix.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRRect.h"

// HTML-canvas arc angle canonicalization. Maps (startAngle, endAngle) to a
// start angle in [0,2pi) and shifts endAngle by the same delta.
void canonicalizeAngle(double *startAngle, double *endAngle);

// Adjust the end angle so the signed difference (endAngle - startAngle) is the
// exact sweep to draw — yielding +/-2pi (full circle) for 0..2pi inputs.
double adjustEndAngle(double startAngle, double endAngle,
                      bool counterclockwise);

// Append a circular arc (center xc,yc, given radius) from angle a1 to a2 to
// `path`. The caller chooses a1/a2 such that (a2 - a1) is the EXACT signed
// sweep. Connects to the current point with a line, or starts the contour.
// `ccw` only documents intent; direction is encoded in a2 - a1.
void path_arc(SkPathBuilder &path, double xc, double yc, double radius,
              double a1, double a2, bool ccw);

// --- High-level primitives (LOCAL coordinates) ----------------------------

// CanvasPath.arc(): canonicalizes angles and appends the arc. Caller is
// responsible for seeding `sub` with the current point (if any) beforehand.
void nxcp_arc(SkPathBuilder &sub, double x, double y, double r,
              double startAngle, double endAngle, bool ccw);

// CanvasPath.ellipse(): builds the ellipse arc by transforming a unit-circle
// arc into LOCAL space via translate(x,y)*rotate(rotation)*scale(rx,ry). The
// returned sub-path is in LOCAL/user space (NOT device space). `p0_local` (if
// has_current) is the current point in LOCAL space, used to seed the unit arc.
void nxcp_ellipse(SkPathBuilder &sub, double x, double y, double rx, double ry,
                  double rotation, double startAngle, double endAngle, bool ccw,
                  bool has_current, SkPoint p0_local);

// CanvasPath.arcTo(): builds the arcTo geometry given the current point `p0`
// (LOCAL space). Caller seeds `sub` with the moveTo(p0) beforehand (matching
// canvas.cc), and this function appends the line/arc segments.
void nxcp_arc_to(SkPathBuilder &sub, SkPoint p0, double x1, double y1,
                 double x2, double y2, double radius);

// CanvasPath.rect(): always starts a new subpath (caller appends, not extends).
void nxcp_rect(SkPathBuilder &sub, double x, double y, double w, double h);

// CanvasPath.roundRect(): builds a rounded rectangle. `radii` are the four
// per-corner radii in SkRRect order (TL, TR, BR, BL) AFTER any negative-
// dimension corner swaps; x/y/w/h are normalized (non-negative w/h). This
// function applies the spec radii down-scale (clamp) and builds the rrect with
// the given winding direction.
void nxcp_round_rect(SkPathBuilder &sub, double x, double y, double w, double h,
                     const SkVector radii[4], bool clockwise);
