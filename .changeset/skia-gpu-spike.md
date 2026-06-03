---
"@nx.js/runtime": patch
---

feat: Phase 2.0 Skia GPU spike — EGL + Ganesh GL surface, validated on hardware in both memory regimes (full-memory + full JIT, applet + jitless). Memory-gate the socket buffer config and main-loop exit so a GPU canvas survives applet mode.
