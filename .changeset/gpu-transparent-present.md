---
"@nx.js/runtime": patch
---

fix: the GPU screen renderer now blits the canvas into the EGL back buffer with `SkBlendMode::kSrc` instead of the default `kSrcOver`, so transparent/partially-transparent pixels fully replace the (double-buffered, stale) back buffer each frame instead of blending it through and looking additive — matching the CPU raster path.
