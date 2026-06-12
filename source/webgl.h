#pragma once
#include "types.h"

// WebGL2 rendering context for the screen canvas, backed by a real OpenGL ES 3
// context (EGL + Mesa/nouveau) over the default NWindow. The app renders
// directly into the EGL window's default framebuffer (FBO 0); the main loop
// presents by calling nx_webgl_present() (eglSwapBuffers) once per frame when
// the drawing buffer was touched since the last present.
//
// Mutually exclusive with the Canvas 2D screen paths (Skia GPU / raster
// framebuffer): the TS side only creates a WebGL2 context when no 2D screen
// context exists (and vice versa), so EGL ownership of the NWindow never
// conflicts with skia_gpu.cc or the libnx framebuffer.

void nx_init_webgl(v8::Isolate *iso, v8::Local<v8::Object> init_obj);

// True when a WebGL2 screen context is active (EGL owns the NWindow).
bool nx_webgl_active(void);

// Present one frame: eglSwapBuffers if the default framebuffer was drawn to
// since the last present, otherwise sleep ~1 vblank to keep loop pacing.
void nx_webgl_present(void);

// Tear down the GL context + EGL. Idempotent.
void nx_webgl_exit(void);

// Implemented by main.cc: release whatever currently owns the default
// NWindow / display (PrintConsole, raster framebuffer, or Skia GPU screen
// path) so the WebGL EGL context can claim it. `screen` is the screen canvas
// wrapper object.
void nx_screen_release_for_webgl(v8::Isolate *iso,
                                 v8::Local<v8::Value> screen);
