# Phase 2: Cairo → Skia (GPU) port plan

Skia (Ganesh GL) **replaces** Cairo as the single rendering backend (one backend,
GPU). Reference: `MIGRATION-NOTES.md` §§269–438 (de-risked trifecta recipe) and
`devkitPro/pacman-packages/examples/trifecta/source-gpu/main.cpp` (working GPU
demo). Memory/JIT policy: `JIT-CANVAS-POLICY.md`.

## Architecture decisions

### Surface backing — Chromium's model
- **Screen**: GPU surface — `SkSurfaces::WrapBackendRenderTarget` over the
  default EGL window's FBO 0 (RGBA8888, `kBottomLeft_GrSurfaceOrigin`).
  Presented via `eglSwapBuffers` (replaces the framebuffer `memcpy`).
- **2D canvases / OffscreenCanvas**: GPU-backed `SkSurface` by default
  (matches Chromium: accelerated 2D canvases).
- **getImageData**: GPU `readPixels` (a flush + GPU→CPU copy), like Chromium.
- **Adaptive demotion**: honor `willReadFrequently` and a readback counter —
  when a canvas is read-heavy, recreate its backing as a **raster** `SkSurface`
  so `getImageData`/`putImageData` are cheap CPU ops.
- This is cheap to support because `SkCanvas` is **backing-agnostic**: every
  drawing method in canvas.cc is identical for GPU vs raster surfaces. Only
  (a) surface creation and (b) `getImageData`/`putImageData`/`toDataURL` branch
  on backing type.

### Type mapping
| Cairo | Skia |
|---|---|
| `cairo_surface_t*` (canvas/image) | `sk_sp<SkSurface>` (canvas) / `sk_sp<SkImage>` (image) |
| `cairo_t*` | `SkCanvas*` (from `surface->getCanvas()`) |
| `cairo_path_t*` / current path | `SkPath` (built incrementally on the context) |
| `cairo_pattern_t*` (gradient/pattern) | `sk_sp<SkShader>` |
| `cairo_matrix_t` | `SkMatrix` / `SkM44` |
| `cairo_font_face_t*` | `sk_sp<SkTypeface>` + `SkFont` |
| `cairo_*_operator` (29 CSS blend modes) | `SkBlendMode` |
| `cairo_filter_t` (image smoothing) | `SkSamplingOptions` |
| shadow: offscreen + box-blur | `SkImageFilters::DropShadow` / `SkMaskFilter` blur |

### Text
Keep **FreeType + HarfBuzz** for face loading + shaping (unchanged). Replace the
Cairo glyph path: build an `SkTextBlob`/glyph run from HarfBuzz output and
`SkCanvas::drawTextBlob` (or feed FT outlines into `SkPath` for strokeText).
Face: `SkFontMgr_New_Custom_Empty()->makeFromData(...)` over the FT_Byte buffer,
or an `SkTypeface` wrapping the existing FT_Face.

### Images
Decode (libpng/turbojpeg/libwebp, premultiplied BGRA) is unchanged. Replace the
`cairo_image_surface_create_for_data` wrap with a raster `SkImage`
(`SkImages::RasterFromPixmap` / `SkImage::MakeRasterData`, BGRA premul).
`drawImage` → `SkCanvas::drawImageRect` with `SkSamplingOptions`.

### Present loop (main.cc)
Replace `framebufferCreate`/`framebufferBegin`+`memcpy`/`framebufferEnd` with:
EGL init on `nwindowGetDefault()` (once, when canvas mode starts) → per frame:
draw → `gr->flush(surface)` + `gr->submit()` → `eglSwapBuffers`. EGL and the
libnx console are mutually exclusive (no console in GPU mode; log to SD).

### JIT policy (main.cc)
Per JIT-CANVAS-POLICY: GPU canvas → full JIT in application mode, **jitless in
applet mode** (the existing `mem_free > 300 MiB` gate already does this).

## Order of work
1. **Spike (2.0):** EGL + Ganesh GL Screen surface in nx.js's present loop; a
   trivial Skia draw (clear + rect + text) each frame; verify on hardware in
   both regimes. De-risks integration before touching canvas.cc.
2. Port `canvas.cc` (the `SkCanvas`/`SkPaint`/`SkPath` body) — backing-agnostic.
3. Surface creation (GPU default + raster fallback) + `getImageData`/
   `putImageData`/`toDataURL` (readPixels/upload).
4. Rewire `font.cc`, `image.cc`, `irs.cc`, drop the `dommatrix` Cairo union.
5. Makefile: link the skia-gl stack (`-DSK_GL`, `-I .../include/skia`,
   `skia-horizon-port.o` + skia libs + EGL/GLESv2/glapi/drm_nouveau + decode
   libs + explicit `-lpng -lz`), drop `cairo`/`pixman`. Update the `version`
   object (drop cairo/pixman, add skia).
6. Build + hardware test (applet jitless + application JIT). Update notes.
