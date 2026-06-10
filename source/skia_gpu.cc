#include "skia_gpu.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <stdio.h>

#include "include/core/SkBlendMode.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkGraphics.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkSurface.h"
#include "include/gpu/GpuTypes.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/egl/GrGLMakeEGLInterface.h"

namespace {

EGLDisplay s_dpy = nullptr;
EGLSurface s_surf = nullptr;
EGLContext s_ctx = nullptr;

sk_sp<GrDirectContext> s_gr;
// The EGL window's FBO 0, double-buffered (the present target).
sk_sp<SkSurface> s_fbo;
// A persistent, single render-target surface the canvas draws into. Canvas 2D
// semantics require ONE persistent surface (apps draw incrementally), but the
// FBO is double-buffered, so we composite: draw into s_canvas (persistent),
// then blit it to s_fbo and swap each frame.
sk_sp<SkSurface> s_canvas;
u32 s_w = 0, s_h = 0;

bool init_egl(NWindow *win) {
	s_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (!s_dpy)
		return false;
	eglInitialize(s_dpy, nullptr, nullptr);
	eglBindAPI(EGL_OPENGL_ES_API);
	EGLConfig cfg;
	EGLint num = 0;
	const EGLint attrs[] = {EGL_RED_SIZE,   8, EGL_GREEN_SIZE,   8,
	                        EGL_BLUE_SIZE,  8, EGL_ALPHA_SIZE,   8,
	                        EGL_DEPTH_SIZE, 24, EGL_STENCIL_SIZE, 8,
	                        EGL_NONE};
	eglChooseConfig(s_dpy, attrs, &cfg, 1, &num);
	if (!num)
		return false;
	s_surf = eglCreateWindowSurface(s_dpy, cfg, win, nullptr);
	if (!s_surf)
		return false;
	const EGLint ca[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
	s_ctx = eglCreateContext(s_dpy, cfg, EGL_NO_CONTEXT, ca);
	if (!s_ctx)
		return false;
	eglMakeCurrent(s_dpy, s_surf, s_surf, s_ctx);
	// NOTE: the EGL window surface is double-buffered. The canvas draws into a
	// separate persistent surface (s_canvas) and we composite it into the back
	// buffer every present (see nx_skia_gpu_present), so double buffering does
	// not cause stale-frame flicker even though the driver doesn't support
	// EGL_SWAP_BEHAVIOR_PRESERVED.
	return true;
}

} // namespace

sk_sp<SkSurface> nx_skia_gpu_screen_init(u32 width, u32 height, int samples,
                                         u32 gpu_cache_mib) {
	if (!init_egl(nwindowGetDefault())) {
		fprintf(stderr, "[skia] EGL init failed\n");
		fflush(stderr);
		nx_skia_gpu_screen_exit();
		return nullptr;
	}
	SkGraphics::Init();
	auto iface = GrGLInterfaces::MakeEGL();
	s_gr = GrDirectContexts::MakeGL(iface);
	if (!s_gr) {
		fprintf(stderr, "[skia] GrDirectContext failed\n");
		fflush(stderr);
		nx_skia_gpu_screen_exit();
		return nullptr;
	}
	// Ganesh's default GPU resource cache budget is only ~96 MiB. A
	// texture-heavy app whose per-frame working set exceeds that (e.g. a
	// full-screen 2D game drawing many large atlases + baked tilemap layers,
	// each up to a few MiB of GPU texture) thrashes the cache: textures still
	// needed next frame get evicted LRU and re-uploaded. When the caller
	// requests a larger budget (gpu_cache_mib > 0; see the regime-gated
	// resolution in main.cc + the [renderer] gpu_cache config), raise it so
	// the working set stays resident. 0 leaves Skia's default untouched
	// (correct for tight-RAM applet mode, where a big cache would starve Mesa).
	if (gpu_cache_mib > 0) {
		const size_t oldBytes = s_gr->getResourceCacheLimit();
		const size_t newBytes = (size_t)gpu_cache_mib * 1024u * 1024u;
		s_gr->setResourceCacheLimit(newBytes);
		fprintf(stderr, "[skia] GPU resource cache limit %zu -> %zu MiB\n",
		        oldBytes / (1024 * 1024), newBytes / (1024 * 1024));
		fflush(stderr);
	}
	GrGLFramebufferInfo fbi;
	fbi.fFBOID = 0;
	fbi.fFormat = 0x8058; // GL_RGBA8
	auto rt = GrBackendRenderTargets::MakeGL((int)width, (int)height, 0, 8, fbi);
	s_fbo = SkSurfaces::WrapBackendRenderTarget(
	    s_gr.get(), rt, kBottomLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType,
	    nullptr, nullptr);
	if (!s_fbo) {
		fprintf(stderr, "[skia] WrapBackendRenderTarget failed\n");
		fflush(stderr);
		nx_skia_gpu_screen_exit();
		return nullptr;
	}
	// Persistent canvas surface (BGRA to match the raster/canvas pixel model
	// and getImageData readback path). Prefer MSAA: Ganesh's coverage-based
	// antialiased path renderer silently drops some large/complex concave fills
	// (e.g. the Ghostscript-tiger mouth) on a non-multisampled target. MSAA
	// rasterizes fills without that renderer and resolves smooth edges, so
	// complex antialiased fills render correctly. MSAA costs samples*WxH*4
	// bytes, which may not fit in applet mode (~137 MiB) -> retry with fewer
	// samples, finally non-MSAA, rather than failing (a failed init would tear
	// down EGL and fall back to the raster path).
	SkImageInfo info = SkImageInfo::Make((int)width, (int)height,
	                                     kBGRA_8888_SkColorType,
	                                     kPremul_SkAlphaType);
	int try_samples[] = {samples, 2, 0};
	for (int si = 0; si < 3 && !s_canvas; si++) {
		int s = try_samples[si];
		if (si > 0 && s >= samples)
			continue;  // don't retry an equal/higher count
		s_canvas = SkSurfaces::RenderTarget(s_gr.get(), skgpu::Budgeted::kNo,
		                                    info, s, kBottomLeft_GrSurfaceOrigin,
		                                    nullptr);
		if (s_canvas) {
			fprintf(stderr, "[skia] canvas surface MSAA=%dx ready\n", s);
			fflush(stderr);
		}
	}
	if (!s_canvas) {
		fprintf(stderr, "[skia] canvas RenderTarget failed (all sample counts)\n");
		fflush(stderr);
		nx_skia_gpu_screen_exit();
		return nullptr;
	}
	s_w = width;
	s_h = height;
	fprintf(stderr, "[skia] GPU screen surface %ux%u ready\n", width, height);
	fflush(stderr);
	return s_canvas;
}

void nx_skia_gpu_present(void) {
	if (!s_canvas || !s_fbo || !s_gr)
		return;
	// Composite: snapshot the persistent canvas surface and blit it into the
	// EGL back buffer (FBO 0), then swap. The persistent surface always holds
	// the full current canvas content, so every presented buffer is correct
	// regardless of double buffering or incremental drawing.
	sk_sp<SkImage> img = s_canvas->makeImageSnapshot();
	if (img) {
		SkCanvas *c = s_fbo->getCanvas();
		// Use kSrc (source-replace), NOT the default kSrcOver: the EGL back
		// buffer is double-buffered and retains stale content (the previous
		// frame, or uninitialized garbage). With kSrcOver, any transparent /
		// partially-transparent pixel in the canvas surface would blend the
		// stale destination through, making output look additive across
		// frames. kSrc copies the surface verbatim — including alpha — so each
		// present fully replaces the back buffer, matching the CPU raster path
		// (which memcpy()s the pixels straight into the framebuffer).
		SkPaint paint;
		paint.setBlendMode(SkBlendMode::kSrc);
		c->drawImage(img, 0, 0, SkSamplingOptions(), &paint);
	}
	s_gr->flush(s_fbo.get());
	s_gr->submit();
	eglSwapBuffers(s_dpy, s_surf);
}

void nx_skia_gpu_screen_exit(void) {
	s_canvas.reset();
	s_fbo.reset();
	if (s_gr) {
		SkGraphics::PurgeAllCaches();
		s_gr.reset();
	}
	// Idempotent: also serves as the cleanup path for a failed init, and main()
	// calls it again at teardown. Null each handle after destroying it.
	if (s_dpy) {
		eglMakeCurrent(s_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (s_ctx) {
			eglDestroyContext(s_dpy, s_ctx);
			s_ctx = nullptr;
		}
		if (s_surf) {
			eglDestroySurface(s_dpy, s_surf);
			s_surf = nullptr;
		}
		eglTerminate(s_dpy);
		s_dpy = nullptr;
	}
}
