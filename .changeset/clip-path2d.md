---
"@nx.js/runtime": patch
---

fix: `CanvasRenderingContext2D.clip(path)` now honors a `Path2D` argument. Previously `clip()` always clipped against the context's current path and treated its first argument only as the fill rule, so `clip(path)` / `clip(path, fillRule)` had no clipping effect (drawing outside the Path2D was not clipped). It now clips against the supplied `Path2D` (baked through the current transform, like `fill(path)`), without disturbing the context's current path, and applies the optional `evenodd`/`nonzero` fill rule.
