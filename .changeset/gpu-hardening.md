---
"@nx.js/runtime": patch
---

fix: harden the Skia GPU canvas backend (Phase 2.2). Composite via a persistent canvas surface (fixes flicker from double-buffering + incremental drawing); use MSAA to fix Ganesh dropping complex concave fills; and choose the renderer per memory regime — GPU + 4x MSAA in application mode, raster in applet mode (no MSAA level fits ~137 MiB, and non-MSAA GPU mis-renders complex fills). Also route the `+` button through the JS frame handler so `preventDefault()` is honored (e.g. pausing instead of exiting). Validated on hardware in both regimes across the canvas/fonts/svg/snake/animated-gif apps.
