---
"@nx.js/runtime": patch
---

feat: GPU-accelerate the screen canvas via Skia Ganesh GL (Phase 2.2). The screen canvas is backed by a GPU SkSurface over the EGL window's FBO 0 and presented with eglSwapBuffers; if GPU bringup fails it falls back to the raster + libnx-framebuffer path. Also fixes the V8 JIT/socket memory gate to key on the process memory grant (TotalMemorySize) instead of (total - used), which mis-detected application mode as low-memory and forced jitless. Validated on hardware: application mode runs full JIT + GPU, applet mode runs jitless + GPU.
