---
"@nx.js/runtime": patch
---

fix: reimplement `Path2D` natively on a Skia `SkPath`, which fixes `addPath(path, transform)` ignoring the `DOMMatrix` argument.
