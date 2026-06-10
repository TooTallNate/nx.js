---
"@nx.js/runtime": patch
---

fix: set `eglSwapInterval(1)` on the GPU screen surface so presentation locks to one buffer-swap per vblank (at whatever rate the display refreshes). Previously no interval was set, leaving the driver default undefined. Also: `eglMakeCurrent` failure during GPU screen init is now detected and fails over to the raster path (it was previously unchecked, so later GL/Ganesh setup could proceed against no current context), and an `eglSwapInterval` failure is logged but non-fatal.
