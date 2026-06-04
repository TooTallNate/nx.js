---
"@nx.js/runtime": patch
---

fix: back `Path2D` with a native Skia `SkPath` instead of a JS command-list polyfill. SVG path strings are parsed by Skia (`SkParsePath`), and `addPath(path, transform)` now correctly applies the optional `DOMMatrix` (it was previously ignored — a Chrome-conformance bug). Path2D coordinates are kept in user space and the canvas transform is baked in when the path is used (fill/stroke/clip/isPointInPath), matching the spec. Adds a `path2d-transform` conformance fixture verified against Chrome. The shared path geometry (arc/ellipse/arcTo/rect/roundRect) was extracted into `canvas_path.{h,cc}` so the context and Path2D share one implementation.
