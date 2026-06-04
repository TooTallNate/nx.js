---
"@nx.js/runtime": patch
---

fix: correctly fill ellipses. `ctx.ellipse()` built the arc from a unit circle whose radius and center were already offset, so the non-uniform scale was applied twice — collapsing the ellipse and leaving its interior unfilled. It now transforms a true unit-circle arc (radius 1, centered at the origin) by `translate * rotate * scale(rx, ry)`. `Path2D.ellipse()` replay also stopped baking the radii into a temporary CTM (which was discarded before the fill); it now calls the `ellipse()` primitive directly.
