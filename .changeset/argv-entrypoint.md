---
"@nx.js/runtime": minor
---

feat: support an explicit entrypoint via `argv[1]` — launch the nx.js runtime with a `.js` file (run directly) or an app `.nro` (its embedded RomFS is mounted as `romfs:` and `romfs:/main.js` is run). The runtime's own files are now mounted under `nxjs:` so `romfs:` always refers to the app. This is the foundation for a thin "bootstrap" launcher that keeps a single shared nx.js runtime on the SD card.
