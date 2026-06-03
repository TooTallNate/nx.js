#include "skia_gpu.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <stdio.h>

#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkGraphics.h"
#include "include/core/SkSurface.h"
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
sk_sp<SkSurface> s_surface;

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
	return true;
}

} // namespace

sk_sp<SkSurface> nx_skia_gpu_screen_init(u32 width, u32 height) {
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
	GrGLFramebufferInfo fbi;
	fbi.fFBOID = 0;
	fbi.fFormat = 0x8058; // GL_RGBA8
	auto rt = GrBackendRenderTargets::MakeGL((int)width, (int)height, 0, 8, fbi);
	s_surface = SkSurfaces::WrapBackendRenderTarget(
	    s_gr.get(), rt, kBottomLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType,
	    nullptr, nullptr);
	if (!s_surface) {
		fprintf(stderr, "[skia] WrapBackendRenderTarget failed\n");
		fflush(stderr);
		nx_skia_gpu_screen_exit();
		return nullptr;
	}
	fprintf(stderr, "[skia] GPU screen surface %ux%u ready\n", width, height);
	fflush(stderr);
	return s_surface;
}

GrDirectContext *nx_skia_gpu_context(void) { return s_gr.get(); }

void nx_skia_gpu_present(void) {
	if (!s_surface || !s_gr)
		return;
	s_gr->flush(s_surface.get());
	s_gr->submit();
	eglSwapBuffers(s_dpy, s_surf);
}

void nx_skia_gpu_screen_exit(void) {
	s_surface.reset();
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
