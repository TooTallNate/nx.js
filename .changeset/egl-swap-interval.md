---
"@nx.js/runtime": patch
---

fix: set `eglSwapInterval(1)` on the GPU screen surface so presentation locks to one buffer-swap per vblank (clean 60Hz cadence). Previously no interval was set, leaving the driver default undefined.
