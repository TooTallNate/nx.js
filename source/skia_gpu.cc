#include "skia_gpu.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <stdio.h>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkData.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkGraphics.h"
#include "include/core/SkPaint.h"
#include "include/core/SkSurface.h"
#include "include/core/SkTypeface.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include "include/gpu/ganesh/gl/egl/GrGLMakeEGLInterface.h"
#include "include/ports/SkFontMgr_empty.h"

namespace {

EGLDisplay s_dpy = nullptr;
EGLSurface s_surf = nullptr;
EGLContext s_ctx = nullptr;

sk_sp<GrDirectContext> s_gr;
sk_sp<SkSurface> s_surface;
sk_sp<SkTypeface> s_typeface;
SkFont s_font;
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
	return true;
}

} // namespace

bool nx_skia_gpu_init(u32 width, u32 height) {
	s_w = width;
	s_h = height;
	if (!init_egl(nwindowGetDefault())) {
		fprintf(stderr, "[skia] EGL init failed\n");
		fflush(stderr);
		nx_skia_gpu_exit();
		return false;
	}
	SkGraphics::Init();
	auto iface = GrGLInterfaces::MakeEGL();
	s_gr = GrDirectContexts::MakeGL(iface);
	if (!s_gr) {
		fprintf(stderr, "[skia] GrDirectContext failed\n");
		fflush(stderr);
		nx_skia_gpu_exit();
		return false;
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
		nx_skia_gpu_exit();
		return false;
	}

	// Load a system font for the HUD (best-effort). The pl session is owned by
	// main() (plInitialize at startup, plExit at teardown), so don't init/exit
	// it here -- doing so would unbalance main's refcount on the failure path.
	PlFontData fd;
	if (R_SUCCEEDED(plGetSharedFontByType(&fd, PlSharedFontType_Standard))) {
		auto data = SkData::MakeWithoutCopy(fd.address, fd.size);
		auto mgr = SkFontMgr_New_Custom_Empty();
		s_typeface = mgr->makeFromData(data);
		if (s_typeface)
			s_font = SkFont(s_typeface, 36);
	}

	fprintf(stderr, "[skia] GPU surface %ux%u ready\n", width, height);
	fflush(stderr);
	return true;
}

void nx_skia_gpu_test_frame(int frame) {
	if (!s_surface)
		return;
	SkCanvas *c = s_surface->getCanvas();
	c->clear(SkColorSetARGB(255, 20, 24, 40));

	// Moving red rect (mirrors the old Cairo smoke test).
	SkPaint p;
	p.setAntiAlias(true);
	float x = (float)((frame * 3) % (int)(s_w - 200));
	p.setColor(SkColorSetARGB(255, 220, 60, 60));
	c->drawRect(SkRect::MakeXYWH(x, 300, 200, 120), p);

	// Stroked green circle.
	p.setColor(SkColorSetARGB(255, 120, 220, 120));
	p.setStyle(SkPaint::kStroke_Style);
	p.setStrokeWidth(6);
	c->drawCircle(640, 200, 80, p);

	if (s_typeface) {
		SkPaint tp;
		tp.setAntiAlias(true);
		tp.setColor(SK_ColorWHITE);
		c->drawString("nx.js on V8 + Skia (GPU)", 360, 120, s_font, tp);
		char buf[64];
		snprintf(buf, sizeof(buf), "frame %d", frame);
		SkFont small(s_typeface, 24);
		c->drawString(buf, 360, 480, small, tp);
	}

	s_gr->flush(s_surface.get());
	s_gr->submit();
	eglSwapBuffers(s_dpy, s_surf);
}

void nx_skia_gpu_exit(void) {
	s_surface.reset();
	s_typeface.reset();
	if (s_gr) {
		SkGraphics::PurgeAllCaches();
		s_gr.reset();
	}
	// Idempotent: this also serves as the cleanup path for a failed
	// nx_skia_gpu_init, and main() calls it again at teardown. Null each handle
	// after destroying it so a second call doesn't double-free stale handles.
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
