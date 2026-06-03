#pragma once
#include <switch.h>

// Phase 2 spike: EGL + Skia Ganesh GL on the default NWindow. This is the seed
// of the GPU rendering backend that replaces Cairo. For the spike it just
// initializes a GPU surface and draws a trivial test scene each frame to prove
// the EGL/Ganesh integration works inside nx.js's present loop.

#ifdef __cplusplus
extern "C" {
#endif

// Initialize EGL + Skia Ganesh GL over the default NWindow at width x height.
// Returns true on success. Must be called on the render (main) thread.
bool nx_skia_gpu_init(u32 width, u32 height);

// Spike: clear + draw a moving rect + HUD text, then flush/submit/swap.
// `frame` is a monotonically increasing frame counter.
void nx_skia_gpu_test_frame(int frame);

// Tear down Skia + EGL.
void nx_skia_gpu_exit(void);

#ifdef __cplusplus
}
#endif
