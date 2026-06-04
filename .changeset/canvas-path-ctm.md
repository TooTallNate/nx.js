---
"@nx.js/runtime": patch
---

fix: canvas paths now bake in the current transform at build time. Per the HTML Canvas spec, each path segment (`moveTo`, `lineTo`, `arc`, `arcTo`, `ellipse`, `rect`, `roundRect`, bezier/quadratic curves) captures the transform (CTM) in effect when it is added, independent of the transform when the path is later filled, stroked, clipped, or hit-tested. Previously the path was stored in user space and the CTM was applied only at draw time, so building a path under one transform and using it under another (e.g. `rect(); translate(); fill()`) drew in the wrong place and `isPointInPath`/`isPointInStroke` returned wrong results. Paths are now stored in device space; fill/clip draw under identity and stroke maps back through the transform so the pen still scales (matching Chrome). Adds `path-transform` conformance + image fixtures verified against Chrome.
