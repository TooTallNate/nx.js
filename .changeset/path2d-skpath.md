---
"@nx.js/runtime": patch
---

fix: reimplement `Path2D` natively on a Skia `SkPath` instead of a JS command-list polyfill. All path methods now live in the native layer (`source/path2d.cc`, installed on the prototype); the TypeScript class is a thin types/TSDoc shell with `stub()` bodies, matching how `CanvasRenderingContext2D` is structured. SVG path strings are parsed by Skia (`SkParsePath`), and `addPath(path, transform)` now correctly applies the optional `DOMMatrix` (it was previously ignored — a Chrome-conformance bug). Path2D coordinates are kept in user space and the canvas transform is baked in when the path is used (fill/stroke/clip/isPointInPath), matching the spec. Adds a `path2d-transform` conformance fixture verified against Chrome. The shared path geometry (arc/ellipse/arcTo/rect/roundRect) was extracted into `canvas_path.{h,cc}` so the context and Path2D share one implementation.
