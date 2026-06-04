---
"@nx.js/runtime": major
---

nx.js v1: re-platform the runtime onto V8 + libuv and Skia.

This is the first v1 (beta) release. The JavaScript engine moves from QuickJS to
**V8** (full JIT, driven by **libuv**), and the Canvas 2D backend moves from
Cairo to **Skia**. These are deep, breaking changes to the runtime internals and
native ABI, hence the major bump.

Highlights:

- **Engine: QuickJS → V8 + libuv.** Full JIT in application mode; jitless in
  applet mode. Memory is gated on the process memory grant (`TotalMemorySize`)
  so application mode correctly runs full JIT and applet mode runs jitless.

- **Canvas 2D: Cairo → Skia.** The entire `CanvasRenderingContext2D`
  implementation — paths, transforms, paint state, text (HarfBuzz + Skia),
  shadows, gradients, images, `ImageData`, `isPointIn*`, and PNG/JPEG/WebP
  encode — now renders on Skia. Cairo and pixman are fully removed.
  `Switch.version` drops `cairo`/`pixman` and adds `skia` (the Skia milestone).

- **GPU-accelerated screen.** The screen canvas is backed by a GPU `SkSurface`
  over the EGL window (Skia Ganesh GL), presented with `eglSwapBuffers`, with a
  raster + libnx-framebuffer fallback. The renderer is chosen per memory regime:
  GPU + 4× MSAA in application mode, raster in applet mode. Compositing uses a
  persistent surface (no double-buffer flicker), and the `+` button is routed
  through the JS frame handler so `preventDefault()` is honored.

- **Correctness fixes** surfaced by the new V8 + Skia conformance harnesses:
  - `DOMException` is imported from the polyfill instead of the global, so
    esbuild no longer renames it to `DOMException2` and registers it under the
    wrong name (affected crypto, websocket, fetch, audio, performance,
    canvas-gradient, abort-controller).
  - `ctx.ellipse()` / `Path2D.ellipse()` fill correctly (the non-uniform scale
    was previously applied twice, collapsing the ellipse interior).
  - `roundRect()` renders all corners correctly via Skia's native `SkRRect`
    (a corner arc previously swept ~270° backwards, carving out a wedge).

Validated on hardware in both memory regimes, and against Chrome via the
TAP and pixel-diff canvas conformance test suites.
