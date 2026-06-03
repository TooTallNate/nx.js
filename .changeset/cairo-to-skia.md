---
"@nx.js/runtime": patch
---

feat: replace Cairo with Skia (raster) as the Canvas 2D rendering backend. The entire CanvasRenderingContext2D implementation (paths, transforms, paint state, text via HarfBuzz+Skia, shadows, gradients, images, ImageData, isPointIn*, and PNG/JPEG/WebP encode) now renders on Skia. Cairo and pixman are fully removed. Validated on hardware. The `Switch.version` object drops `cairo`/`pixman` and adds `skia` (the Skia milestone).
