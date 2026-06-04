---
"@nx.js/runtime": patch
---

fix: `roundRect()` carved a circular wedge out of one corner. The corner arcs were drawn manually with quarter-circle sweeps whose angles wrapped incorrectly around 0/2π, so one corner swept ~270° backwards instead of 90°. It now builds the shape with Skia's native rounded-rect primitive (`SkRRect`), which renders all corners (including per-corner and elliptical radii) correctly.
