---
"@nx.js/runtime": patch
---

feat: new `[renderer] gpu_cache` nxjs.ini key to set the Ganesh GPU resource-cache budget (MiB). Skia's own default is ~96 MiB; a texture-heavy app whose per-frame working set exceeds that thrashes the cache (LRU-evict + re-upload of textures still needed the next frame). Values: `auto` (default — 512 MiB in full-memory Application mode, Skia default in tight-RAM applet mode so a big cache cannot starve Mesa), `default` (always Skia default), or an explicit MiB number (0-4096).
