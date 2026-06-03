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
1. **Spike (2.0): DONE.** EGL + Ganesh GL Screen surface in nx.js's present loop;
   trivial Skia draw each frame; verified on hardware in both regimes (commit
   bc7ccd1). De-risked integration before touching canvas.cc.

### Staging decision (Phase 2.1): raster-first, GPU later

`SkCanvas` is backing-agnostic, so we port the drawing body against a **raster**
`SkSurface` first — for ALL canvases (screen, OffscreenCanvas, Image). This keeps
`nx_canvas_t`'s `data` buffer (kBGRA_8888 / premul, byte layout identical to
today's CAIRO_FORMAT_ARGB32), so:
- `getImageData`/`putImageData` stay cheap CPU ops (`readPixels`/`writePixels`
  over the raster pixmap), no `GrDirectContext` readback.
- The screen present path stays the existing pixel `memcpy` into the framebuffer
  (the 2.0 spike's native-draw present is retired; the screen canvas is a normal
  raster `SkSurface` whose pixels are blitted each frame).
- Drawing logic is **pixel-parity** with the Cairo body, verifiable against the
  existing canvas/svg/snake/animated-gif apps + conformance tests.

GPU-backing (default GPU `SkSurface`, `readPixels`-based `getImageData`,
`willReadFrequently` + readback-counter demotion, `eglSwapBuffers` present) is a
**follow-up (Phase 2.2)** layered on top once raster parity is proven. The
canvas.cc body written now does not change for GPU; only surface creation + the
three pixel-transfer methods + the present path branch on backing.

### Phase 2.1 slices (each build-verified before the next)
2. **Struct + lifecycle:** `canvas.h`/`types.h` Skia state (`sk_sp<SkSurface>`,
   `SkPath`, expanded `nx_canvas_context_2d_state_t` holding everything Cairo
   tracked implicitly: line width/cap/join/miter, dash, blend mode, fill rule,
   shadow, font, gradients as `sk_sp<SkShader>`/stop buffers); `ensure_surface`,
   `new`/`free`, `save`/`restore`.
3. **Primitives + paint + transforms:** path building, fill/stroke/clip/rect,
   arc/ellipse/roundRect, gradients, line styling, globalAlpha, blend modes.
4. **Text:** HarfBuzz shaping (kept) → `SkTextBlob`/`drawGlyphs`; strokeText via
   `SkFont::getPath`; measureText.
5. **Images + ImageData:** `drawImage`→`drawImageRect`; get/put ImageData via
   raster pixmap; encode (PNG via `SkPngEncoder`, JPEG/WebP unchanged); toDataURL.
6. Rewire `font.cc`, `image.cc`, `irs.cc`, drop the `dommatrix` Cairo union.
7. Makefile: link the skia-gl stack, drop `cairo`/`pixman`. Update the `version`
   object (drop cairo/pixman, add skia).
8. Build + hardware test (applet jitless + application JIT). Update notes.

### Phase 2.2 (follow-up): GPU backing + Chromium readback model
GPU-backed default `SkSurface`, `getImageData` via `GrDirectContext` `readPixels`,
`willReadFrequently` + readback counter → raster demotion, screen present via
`eglSwapBuffers`.
