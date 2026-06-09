---
"@nx.js/runtime": patch
---

fix: memoize the wrapped `SkImage` for decoded images so repeat `drawImage()` of the same source no longer re-uploads its pixels to the GPU every call. Previously each `drawImage(image, …)` rebuilt a fresh `SkImage` from the raw pixels, giving every call a new image identity that defeated Ganesh's GPU texture cache (a full source re-upload per draw — measured ~8 ms for a 1024² source, ~25 ms for 2048², regardless of destination size), and at high draw counts (e.g. ~10k tiles/frame) the per-call pixel copy also exhausted the single-threaded GC. The `SkImage` is now built once and cached on the image (a stable identity Ganesh can cache the upload for), released when the image is freed, and invalidated when its pixels are written in place (e.g. IR-camera updates).
