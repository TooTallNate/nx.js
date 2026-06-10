#pragma once
#include <switch.h>

#include "include/core/SkRefCnt.h"
#include "include/core/SkSurface.h"

// Phase 2.2 GPU backend: EGL + Skia Ganesh GL over the default NWindow. The
// screen canvas is backed by a GPU SkSurface wrapping the EGL window's FBO 0
// and presented via eglSwapBuffers. Offscreen canvases remain raster. If GPU
// bringup fails (e.g. Mesa starved for memory in applet mode), the caller falls
// back to the raster + libnx-framebuffer present path.
//
// All functions must be called on the render (main) thread.

// Initialize EGL + a shared GrDirectContext + the screen draw surface at
// width x height. `samples` is the desired MSAA sample count (e.g. 4 in
// application mode, 2 in applet mode); if the multisampled surface can't be
// allocated it automatically retries with no MSAA.
//
// The screen canvas draws into a PERSISTENT offscreen surface;
// `nx_skia_gpu_present` snapshots+blits it into the double-buffered swapchain
// each frame, preserving prior-frame content (standard incremental-canvas
// semantics). The per-frame composite is cheap (measured ~6-8ms at 720p on
// Switch hardware) and is not a meaningful cost.
//
// `gpu_cache_mib` sets the Ganesh GPU resource-cache budget in MiB; 0 leaves
// Skia's own default (~96 MiB). Raise it for a texture-heavy app whose
// per-frame working set would otherwise thrash the cache.
//
// Returns the surface the screen canvas should draw into, or nullptr on
// failure (all partial EGL/Mesa state torn down). The surface is owned by
// this module; do not outlive nx_skia_gpu_screen_exit().
sk_sp<SkSurface> nx_skia_gpu_screen_init(u32 width, u32 height, int samples,
                                         u32 gpu_cache_mib);

// Flush + submit the GPU surface and eglSwapBuffers (present one frame).
void nx_skia_gpu_present(void);

// Tear down the GPU surface, GrDirectContext, and EGL. Idempotent.
void nx_skia_gpu_screen_exit(void);
