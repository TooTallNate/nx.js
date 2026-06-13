#include <errno.h>
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <ada.h>
#include <ft2build.h>
#include <hb.h>
#include <libplatform/libplatform.h>
#include <mbedtls/version.h>
#include <png.h>
#include <switch.h>
#include <turbojpeg.h>
#include <uv.h>
#include <v8.h>
#include <webp/decode.h>
#include <zlib.h>
#include <zstd.h>
#include FT_FREETYPE_H

#include "error.h"
#include "hidsys.h"
#include "module.h"
#include "skia_gpu.h"
#include "types.h"
#include "util.h"
#include "webgl.h"

#include "include/core/SkMilestone.h"
#include "include/core/SkSurface.h"

// Phase 2.2: the screen canvas is backed by a GPU SkSurface (EGL FBO 0) and
// presented via eglSwapBuffers when GPU bringup succeeds. If it fails (e.g.
// Mesa starved for memory in applet mode), we fall back to a raster SkSurface
// presented by blitting into the libnx framebuffer (the proven Phase 2.1
// path). `screen_is_gpu` records which path is active for the present loop.
static bool screen_is_gpu = false;
// Memory regime, set in main() before the loop: true in applet mode (~512 MiB
// grant). Gates GPU MSAA sample count (applet can't afford 4x MSAA).
static bool g_tight_memory = false;

// Module init declarations (each defined in its own .cc). Signature per
// BINDINGS.md: void nx_init_foo(v8::Isolate*, v8::Local<v8::Object> init_obj).
#define NX_MODULE(name)                                                        \
	void nx_init_##name(v8::Isolate *, v8::Local<v8::Object>)
NX_MODULE(account);
NX_MODULE(album);
NX_MODULE(applet);
NX_MODULE(audio);
NX_MODULE(battery);
NX_MODULE(bluetooth);
NX_MODULE(canvas);
NX_MODULE(compression);
NX_MODULE(crypto);
NX_MODULE(dns);
NX_MODULE(dommatrix);
NX_MODULE(error);
NX_MODULE(font);
NX_MODULE(fs);
NX_MODULE(fsdev);
NX_MODULE(gamepad);
NX_MODULE(image);
NX_MODULE(irs);
NX_MODULE(memory);
NX_MODULE(nifm);
NX_MODULE(ns);
NX_MODULE(path2d);
NX_MODULE(service);
NX_MODULE(swkbd);
NX_MODULE(tcp);
NX_MODULE(tls);
NX_MODULE(udp);
NX_MODULE(url);
NX_MODULE(video);
NX_MODULE(web);
NX_MODULE(window);
#undef NX_MODULE

using namespace v8;

#define LOG_FILENAME "nxjs-debug.log"

// runtime.js source, embedded as a byte array by the build (runtime_js.c).
extern "C" const unsigned char nxjs_runtime_js[];
extern "C" const unsigned int nxjs_runtime_js_len;

// switch-v8: release manual svcMapMemory arenas before returning to hbloader.
extern "C" void horizon_mman_teardown(void);
// switch-v8: tune the JIT code-arena budget BEFORE V8 init. wasm_headroom_mb is
// extra code space for WebAssembly beyond V8's 64 MiB code-range floor; since
// the libnx jit_* arena is dual-mapped (rx+rw) each MiB costs ~2 MiB real, so
// 0 keeps the arena at the ~64 MiB minimum (~128 MiB real) instead of growing
// it. max_code_mb is a hard ceiling (0 = automatic). See the headroom logic in
// main() and apps/wasm/romfs/nxjs.ini.
extern "C" void horizon_mman_set_code_budget(size_t wasm_headroom_mb,
                                             size_t max_code_mb);

// switch-v8: address space the mman reserved (from the STACK region) for V8's
// object heap, in bytes (typically ~1 GiB; 0 if the reservation failed). Used to
// size the V8 max heap against the real committable ceiling.
extern "C" size_t horizon_mman_data_arena_size(void);

// ---------------------------------------------------------------------------
// Rendering state (Phase 1: Cairo raster -> libnx framebuffer memcpy).
// ---------------------------------------------------------------------------
static PrintConsole *print_console = NULL;
static NWindow *win = NULL;
static Framebuffer *framebuffer = NULL;
static uint8_t *js_framebuffer = NULL;
static u32 js_fb_width = 0;
static u32 js_fb_height = 0;
static int is_running = 1;

// Applet-regime display funding (see main() for the rationale):
//  - g_nv_early_ref: we hold a process-lifetime nvdrv reference taken at boot
//    (when memory is plentiful). Without it, the boot console's
//    framebufferClose() drops the nv refcount to zero and the canvas-mode
//    framebufferCreate() re-opens nvdrv:a — a close→reopen cycle that was
//    observed to HANG inside nvservices' Initialize when applet memory is
//    nearly exhausted (the 3 MiB transfer-memory + session re-setup).
//  - g_display_parachute: ~12 MiB reserved at boot in the applet regime and
//    released right before the raster framebuffer is created, guaranteeing
//    the display path stays fundable no matter how much the JS runtime grew
//    in the meantime (applet + JIT leaves only single-digit MiB of slack).
static bool g_nv_early_ref = false;
static void *g_display_parachute = NULL;

static void nx_display_parachute_release(void) {
	if (g_display_parachute != NULL) {
		free(g_display_parachute);
		g_display_parachute = NULL;
	}
}

void nx_console_init(nx_context_t *nx_ctx) {
	nx_ctx->rendering_mode = NX_RENDERING_MODE_CONSOLE;
	if (print_console == NULL) {
		print_console = consoleInit(NULL);
	}
}

void nx_console_exit() {
	if (print_console != NULL) {
		consoleExit(print_console);
		print_console = NULL;
	}
}

void nx_framebuffer_exit() {
	if (framebuffer != NULL) {
		framebufferClose(framebuffer);
		free(framebuffer);
		framebuffer = NULL;
		js_framebuffer = NULL;
	}
}

void nx_exit_event_loop(void) { is_running = 0; }

// nx_get_canvas is provided by canvas.cc; forward-declare to wire framebuffer.
struct nx_canvas_s;
nx_canvas_s *nx_get_canvas(Isolate *iso, Local<Value> obj);
// Accessors into the opaque canvas struct (defined in canvas.cc).
uint8_t *nx_canvas_pixels(nx_canvas_s *c);
u32 nx_canvas_width(nx_canvas_s *c);
u32 nx_canvas_height(nx_canvas_s *c);
void nx_canvas_set_gpu_surface(nx_canvas_s *c, sk_sp<SkSurface> surface);
void nx_canvas_release_gpu_surface(nx_canvas_s *c);

// Memory-regime accessor for webgl.cc (g_tight_memory is static here).
bool nx_tight_memory(void) { return g_tight_memory; }

// Called by webgl.cc before bringing up its own EGL/ES3 context: release
// whatever currently owns the default NWindow / display path — the libnx
// PrintConsole, the raster framebuffer, or the Skia GPU (EGL) screen path.
// `screen` is the screen canvas wrapper (demoted back to raster backing when
// it had adopted the Skia GPU surface, so a stale 2D context can't draw into
// a surface whose GrDirectContext is destroyed).
void nx_screen_release_for_webgl(Isolate *iso, Local<Value> screen) {
	nx_console_exit();
	if (screen_is_gpu) {
		nx_canvas_s *canvas = nx_get_canvas(iso, screen);
		if (canvas) {
			nx_canvas_release_gpu_surface(canvas);
		}
		nx_skia_gpu_screen_exit();
		screen_is_gpu = false;
	} else {
		nx_framebuffer_exit();
	}
}

// ---------------------------------------------------------------------------
// Core `$` functions implemented here in main.cc.
// ---------------------------------------------------------------------------

static void js_exit(const FunctionCallbackInfo<Value> &info) {
	is_running = 0;
}

// queueMicrotask(callback): schedule `callback` to run as a V8 microtask (i.e.
// before control returns to the event loop, after the current task). Raw V8
// does not install a `queueMicrotask` global (it's a Blink/Node binding), so we
// provide it here to match the WHATWG `queueMicrotask()` and restore the
// QuickJS-era global. The retained callback is stored in a heap Global<Function>
// that the microtask frees after invocation.
static void nx_microtask_run(void *data) {
	Isolate *iso = Isolate::GetCurrent();
	HandleScope scope(iso);
	Local<Context> context = iso->GetCurrentContext();
	Context::Scope ctx_scope(context);

	Global<Function> *gfn = static_cast<Global<Function> *>(data);
	Local<Function> fn = gfn->Get(iso);

	TryCatch try_catch(iso);
	Local<Value> ret;
	if (!fn->Call(context, Undefined(iso), 0, nullptr).ToLocal(&ret)) {
		// WHATWG: "If the callback throws an exception, report the exception."
		// nx_emit_error_event dispatches the global `error` event (and falls
		// back to printing + flagging had_error if no handler prevents it).
		nx_emit_error_event(iso, &try_catch);
	}

	gfn->Reset();
	delete gfn;
}

static void js_queue_microtask(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	if (info.Length() < 1 || !info[0]->IsFunction()) {
		nx_throw(iso,
		         "Failed to execute 'queueMicrotask': parameter 1 is not of "
		         "type 'Function'.");
		return;
	}
	Global<Function> *gfn =
	    new Global<Function>(iso, info[0].As<Function>());
	iso->EnqueueMicrotask(nx_microtask_run, gfn);
}

static void js_print(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_context_t *ctx = nx_ctx(iso);
	if (ctx->rendering_mode != NX_RENDERING_MODE_CONSOLE) {
		nx_framebuffer_exit();
		nx_console_init(ctx);
	}
	String::Utf8Value str(iso, info[0]);
	if (*str) {
		printf("%s", *str);
	}
}

static void js_print_err(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value str(iso, info[0]);
	if (*str) {
		fprintf(stderr, "%s", *str);
	}
}

static void js_cwd(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	char cwd[1024];
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		char final_path[1036];
		if (cwd[0] == '/') {
			snprintf(final_path, sizeof(final_path), "sdmc:%s", cwd);
		} else {
			snprintf(final_path, sizeof(final_path), "%s", cwd);
		}
		size_t len = strlen(final_path);
		if (len > 0 && final_path[len - 1] != '/') {
			final_path[len] = '/';
			final_path[len + 1] = '\0';
		}
		info.GetReturnValue().Set(nx_str(iso, final_path));
	}
}

static void js_chdir(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value dir(iso, info[0]);
	if (!*dir || chdir(*dir) != 0) {
		nx_throw_errno_error(iso, errno, "chdir");
	}
}

static void js_getenv(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value name(iso, info[0]);
	if (!*name)
		return;
	char *value = getenv(*name);
	if (value != NULL) {
		info.GetReturnValue().Set(nx_str(iso, value));
	}
}

static void js_setenv(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value name(iso, info[0]);
	String::Utf8Value value(iso, info[1]);
	if (!*name || !*value || setenv(*name, *value, 1) != 0) {
		nx_throw_errno_error(iso, errno, "setenv");
	}
}

static void js_unsetenv(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value name(iso, info[0]);
	if (!*name || unsetenv(*name) != 0) {
		nx_throw_errno_error(iso, errno, "unsetenv");
	}
}

static void js_env_to_object(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	Local<Object> env = Object::New(iso);
	extern char **environ;
	for (char **envp = environ; *envp; envp++) {
		char *key = strdup(*envp);
		if (!key)
			continue;
		char *eq = strchr(key, '=');
		if (eq) {
			*eq = '\0';
			env->Set(context, nx_str_lossy(iso, key), nx_str_lossy(iso, eq + 1))
			    .Check();
		}
		free(key);
	}
	info.GetReturnValue().Set(env);
}

static void nx_set_frame_handler(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_ctx(iso)->frame_handler.Reset(iso, info[0].As<Function>());
}

static void nx_set_exit_handler(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_ctx(iso)->exit_handler.Reset(iso, info[0].As<Function>());
}

// Returns [state, result] for a Promise (Switch.inspect uses this).
static void js_get_internal_promise_state(
    const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	if (!info[0]->IsPromise()) {
		return;
	}
	Local<Promise> promise = info[0].As<Promise>();
	Local<Array> arr = Array::New(iso, 2);
	Promise::PromiseState state = promise->State();
	arr->Set(context, 0, Integer::New(iso, state)).Check();
	if (state == Promise::kPending) {
		arr->Set(context, 1, Null(iso)).Check();
	} else {
		arr->Set(context, 1, promise->Result()).Check();
	}
	info.GetReturnValue().Set(arr);
}

static void nx_framebuffer_init(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_context_t *ctx = nx_ctx(iso);
	nx_console_exit();
	if (win == NULL) {
		win = nwindowGetDefault();
	}
	if (framebuffer != NULL) {
		framebufferClose(framebuffer);
		free(framebuffer);
	}
	nx_canvas_s *canvas = nx_get_canvas(iso, info[0]);
	if (!canvas) {
		return; // nx_get_canvas threw
	}
	u32 width = nx_canvas_width(canvas);
	u32 height = nx_canvas_height(canvas);

	// Phase 2.2: renderer choice is per memory regime, using whichever is both
	// correct AND fits:
	//   - application mode (GiBs free): GPU (EGL FBO 0) with 4x MSAA. MSAA is
	//     required because Ganesh's coverage-AA path renderer drops some large
	//     concave fills without it; 4x fits easily here.
	//   - applet mode (~137 MiB): raster. No MSAA level fits in applet memory,
	//     and non-MSAA GPU drops those complex fills, so GPU there would be
	//     incorrect. Raster is fully correct and fits. (This also avoids the
	//     EGL-mid-loop HID issue that only affected applet+GPU.)
	// A failed GPU init in application mode still falls back to raster below.
	//
	// nxjs.ini [renderer] mode overrides this: `auto` keeps the regime default;
	// `cpu` forces raster; `gpu` attempts GPU even in applet mode. GPU init that
	// returns null ALWAYS falls back to raster (never a hard failure), so `gpu`
	// can't crash startup.
	bool want_gpu;
	switch (ctx->config.renderer) {
	case NX_RENDER_CPU:
		want_gpu = false;
		break;
	case NX_RENDER_GPU:
		want_gpu = true;
		break;
	case NX_RENDER_AUTO:
	default:
		want_gpu = !g_tight_memory;
		break;
	}
	// Resolve the GPU resource-cache budget (MiB; 0 = leave Skia's default).
	// AUTO: 512 MiB in full-memory mode (ample headroom for a texture-heavy
	// app's working set), Skia default in applet mode (tight RAM — a big cache
	// would starve Mesa). An explicit `[renderer] gpu_cache` overrides.
	uint32_t gpu_cache_mib = ctx->config.gpu_cache_mib;
	if (gpu_cache_mib == NX_GPU_CACHE_AUTO)
		gpu_cache_mib = g_tight_memory ? 0u : 512u;
	sk_sp<SkSurface> gpu =
	    want_gpu ? nx_skia_gpu_screen_init(width, height, /*samples=*/4,
	                                       gpu_cache_mib)
	             : nullptr;
	if (gpu) {
		nx_canvas_set_gpu_surface(canvas, gpu);
		screen_is_gpu = true;
		js_framebuffer = nullptr;
		// Bringing up EGL/Mesa (which claims the NWindow + nvidia driver
		// resources) can leave HID not delivering input when it runs AFTER the
		// pads were configured — notably for async entry modules (top-level
		// await), where getContext()/GPU init runs mid-loop rather than before
		// it. Re-configure + re-initialize the pads here so button input flows
		// regardless of when GPU init happened.
		padConfigureInput(8, HidNpadStyleSet_NpadStandard |
		                         HidNpadStyleTag_NpadGc);
		padInitializeDefault(&ctx->pads[0]);
		for (int i = 1; i < 8; i++) {
			padInitialize(&ctx->pads[i], (HidNpadIdType)(HidNpadIdType_No1 + i));
		}
	} else {
		// Explain why raster was chosen. If the app explicitly asked for GPU
		// but init failed, log it as a config value that wasn't honored.
		if (ctx->config.renderer == NX_RENDER_GPU) {
			fprintf(stderr,
			        "[config] renderer.mode=gpu not honored: GPU/EGL init "
			        "failed, falling back to CPU raster\n");
		} else {
			fprintf(stderr, "[skia] using raster path (%s)\n",
			        ctx->config.renderer == NX_RENDER_CPU
			            ? "renderer.mode=cpu"
			            : (g_tight_memory ? "applet regime"
			                              : "GPU init failed, falling back"));
		}
		fflush(stderr);
		screen_is_gpu = false;
		js_framebuffer = nx_canvas_pixels(canvas);
		js_fb_width = width;
		js_fb_height = height;
		// Release the display parachute (reserved at boot in the applet
		// regime) so the libnx framebuffer allocations below — ~7.5 MiB
		// block-linear swapchain + ~3.7 MiB linear staging buffer — are
		// guaranteed to be fundable even when the rest of applet memory is
		// exhausted (applet + JIT leaves single-digit MiB of slack; see the
		// parachute allocation in main()).
		nx_display_parachute_release();
		framebuffer = (Framebuffer *)malloc(sizeof(Framebuffer));
		Result fbrc = framebufferCreate(framebuffer, win, width, height,
		                                PIXEL_FORMAT_BGRA_8888, 2);
		if (R_SUCCEEDED(fbrc)) {
			fbrc = framebufferMakeLinear(framebuffer);
		}
		if (R_FAILED(fbrc)) {
			// Out of memory (or display in a bad state): present nothing
			// rather than crash. The loop's framebufferBegin() null-guard
			// skips the blit; the app keeps running and `+` still exits.
			fprintf(stderr,
			        "[fb] raster framebuffer init failed: 0x%x (out of "
			        "memory?); the screen will stay black\n",
			        fbrc);
			fflush(stderr);
			framebufferClose(framebuffer);
			free(framebuffer);
			framebuffer = NULL;
		}
	}
	ctx->rendering_mode = NX_RENDERING_MODE_CANVAS;
}

// HID touch/keyboard/vibration — straightforward marshalling.
static void js_hid_initialize_touch_screen(
    const FunctionCallbackInfo<Value> &info) {
	hidInitializeTouchScreen();
}

static void js_hid_initialize_keyboard(
    const FunctionCallbackInfo<Value> &info) {
	hidInitializeKeyboard();
}

static void js_hid_get_touch_screen_states(
    const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	HidTouchScreenState state = {0};
	hidGetTouchScreenStates(&state, 1);
	if (state.count == 0) {
		return;
	}
	Local<Array> arr = Array::New(iso, state.count);
	for (int i = 0; i < state.count; i++) {
		Local<Object> touch = Object::New(iso);
		Local<Value> x = Integer::New(iso, state.touches[i].x);
		Local<Value> y = Integer::New(iso, state.touches[i].y);
		touch->Set(context, nx_str(iso, "identifier"),
		           Integer::New(iso, state.touches[i].finger_id))
		    .Check();
		touch->Set(context, nx_str(iso, "clientX"), x).Check();
		touch->Set(context, nx_str(iso, "clientY"), y).Check();
		touch->Set(context, nx_str(iso, "screenX"), x).Check();
		touch->Set(context, nx_str(iso, "screenY"), y).Check();
		touch->Set(context, nx_str(iso, "radiusX"),
		           Number::New(iso, state.touches[i].diameter_x / 2.0))
		    .Check();
		touch->Set(context, nx_str(iso, "radiusY"),
		           Number::New(iso, state.touches[i].diameter_y / 2.0))
		    .Check();
		touch->Set(context, nx_str(iso, "rotationAngle"),
		           Integer::New(iso, state.touches[i].rotation_angle))
		    .Check();
		arr->Set(context, i, touch).Check();
	}
	info.GetReturnValue().Set(arr);
}

static void js_hid_get_keyboard_states(
    const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	HidKeyboardState state = {0};
	hidGetKeyboardStates(&state, 1);
	Local<Object> obj = Object::New(iso);
	obj->Set(context, nx_str(iso, "modifiers"),
	         BigInt::NewFromUnsigned(iso, state.modifiers))
	    .Check();
	for (int i = 0; i < 4; i++) {
		obj->Set(context, i, BigInt::NewFromUnsigned(iso, state.keys[i]))
		    .Check();
	}
	info.GetReturnValue().Set(obj);
}

static void js_hid_initialize_vibration_devices(
    const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_context_t *ctx = nx_ctx(iso);
	Result rc = hidInitializeVibrationDevices(
	    ctx->vibration_device_handles, 2, HidNpadIdType_Handheld,
	    HidNpadStyleSet_NpadStandard);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "hidInitializeVibrationDevices");
	}
}

static void js_hid_send_vibration_values(
    const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	nx_context_t *ctx = nx_ctx(iso);
	Local<Object> v = info[0].As<Object>();
	double low_amp = 0, low_freq = 0, high_amp = 0, high_freq = 0;
	auto get_num = [&](const char *key, double *out) {
		Local<Value> val;
		if (v->Get(context, nx_str(iso, key)).ToLocal(&val)) {
			if (!val->NumberValue(context).To(out)) {
				*out = 0;
			}
		}
	};
	get_num("lowAmp", &low_amp);
	get_num("lowFreq", &low_freq);
	get_num("highAmp", &high_amp);
	get_num("highFreq", &high_freq);
	HidVibrationValue values[2];
	values[0].freq_low = low_freq;
	values[0].amp_low = low_amp;
	values[0].freq_high = high_freq;
	values[0].amp_high = high_amp;
	memcpy(&values[1], &values[0], sizeof(HidVibrationValue));
	Result rc =
	    hidSendVibrationValues(ctx->vibration_device_handles, values, 2);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "hidSendVibrationValues");
	}
}

// ---------------------------------------------------------------------------
// File reading helper.
// ---------------------------------------------------------------------------

// Case-insensitive check that `path` ends with `suffix` (e.g. ".nro").
static bool path_has_suffix(const char *path, const char *suffix) {
	size_t plen = strlen(path);
	size_t slen = strlen(suffix);
	return plen >= slen && strcasecmp(path + plen - slen, suffix) == 0;
}

// Mount the embedded RomFS of the NRO at `path` under device `name`. An NRO's
// RomFS is not at file offset 0: it lives after the code segments + asset
// header. romfsMountFromFsdev() takes the romfs start offset but does NOT parse
// the NRO, so compute the offset here (mirroring libnx's romfsMountSelf): read
// the NroHeader (which follows the 16-byte NroStart, so at offset
// sizeof(NroStart)) to get `size`, then the NroAssetHeader at `size` to get the
// romfs section offset. Returns a libnx Result.
static Result mount_nro_romfs(const char *path, const char *name) {
	FILE *f = fopen(path, "rb");
	if (!f)
		return MAKERESULT(Module_Libnx, LibnxError_NotFound);
	NroHeader hdr;
	NroAssetHeader asset;
	Result rc = MAKERESULT(Module_Libnx, LibnxError_IoError);
	// NroHeader follows NroStart (16 bytes) at the start of the file.
	if (fseek(f, sizeof(NroStart), SEEK_SET) != 0 ||
	    fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr) ||
	    hdr.magic != NROHEADER_MAGIC)
		goto done;
	// The asset header is appended right after the NRO code image (hdr.size).
	if (fseek(f, hdr.size, SEEK_SET) != 0 ||
	    fread(&asset, 1, sizeof(asset), f) != sizeof(asset) ||
	    asset.magic != NROASSETHEADER_MAGIC || asset.romfs.offset == 0 ||
	    asset.romfs.size == 0)
		goto done;
	fclose(f);
	// romfsMountFromFsdev opens its own handle; pass the absolute romfs offset.
	return romfsMountFromFsdev(path, hdr.size + asset.romfs.offset, name);
done:
	fclose(f);
	return rc;
}

uint8_t *read_file(const char *filename, size_t *out_size) {
	FILE *file = fopen(filename, "rb");
	if (file == NULL)
		return NULL;
	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	rewind(file);
	uint8_t *buffer = (uint8_t *)malloc(size + 1);
	if (buffer == NULL) {
		fclose(file);
		return NULL;
	}
	size_t result = fread(buffer, 1, size, file);
	fclose(file);
	if (result != size) {
		free(buffer);
		return NULL;
	}
	*out_size = size;
	buffer[size] = '\0';
	return buffer;
}

static bool delete_if_empty(const char *filename) {
	FILE *file = fopen(filename, "rb");
	if (!file)
		return false;
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fclose(file);
	if (size == 0) {
		return remove(filename) == 0;
	}
	return true;
}

// ---------------------------------------------------------------------------
// Switch.version lazy getters (ams / emummc require the SPL service, which we
// initialize on first access and tear down at exit).
// ---------------------------------------------------------------------------

static void nx_version_get_ams(Local<Name>, const PropertyCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	if (!hosversionIsAtmosphere()) {
		info.GetReturnValue().SetUndefined();
		return;
	}
	nx_context_t *ctx = nx_ctx(iso);
	if (!ctx->spl_initialized) {
		ctx->spl_initialized = true;
		splInitialize();
	}
	u64 packed_version;
	Result rc = splGetConfig((SplConfigItem)65000, &packed_version);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "splGetConfig(ExosphereApiVersion)");
		return;
	}
	u8 major = (packed_version >> 56) & 0xFF;
	u8 minor = (packed_version >> 48) & 0xFF;
	u8 micro = (packed_version >> 40) & 0xFF;
	char version_str[12];
	snprintf(version_str, sizeof(version_str), "%u.%u.%u", major, minor, micro);
	info.GetReturnValue().Set(nx_str(iso, version_str));
}

static void nx_version_get_emummc(Local<Name>, const PropertyCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	if (!hosversionIsAtmosphere()) {
		info.GetReturnValue().SetUndefined();
		return;
	}
	nx_context_t *ctx = nx_ctx(iso);
	if (!ctx->spl_initialized) {
		ctx->spl_initialized = true;
		splInitialize();
	}
	u64 is_emummc;
	Result rc = splGetConfig((SplConfigItem)65007, &is_emummc);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "splGetConfig(ExosphereEmummcType)");
		return;
	}
	info.GetReturnValue().Set(Boolean::New(iso, is_emummc ? true : false));
}

// ---------------------------------------------------------------------------
// Module / script evaluation helpers.
// ---------------------------------------------------------------------------

// Compile + run a classic script (used for the embedded runtime.js).
static bool run_script(Isolate *iso, Local<Context> context, const char *src,
                       size_t len, const char *name) {
	HandleScope scope(iso);
	TryCatch try_catch(iso);
	// NewFromUtf8 returns an empty MaybeLocal if the source exceeds
	// String::kMaxLength or a string allocation fails (e.g. tight applet
	// memory). Don't .ToLocalChecked() — that would abort the process; report
	// the failure instead.
	Local<String> source;
	if (!String::NewFromUtf8(iso, src, NewStringType::kNormal, (int)len)
	         .ToLocal(&source)) {
		nx_console_init(nx_ctx(iso));
		fprintf(stderr,
		        "Failed to load %s: source is %zu bytes, which exceeds V8's "
		        "maximum string length\n",
		        name, len);
		return false;
	}
	ScriptOrigin origin(nx_str(iso, name));
	Local<Script> script;
	if (!Script::Compile(context, source, &origin).ToLocal(&script)) {
		print_js_error(iso, &try_catch);
		return false;
	}
	Local<Value> result;
	if (!script->Run(context).ToLocal(&result)) {
		print_js_error(iso, &try_catch);
		return false;
	}
	return true;
}


// ---------------------------------------------------------------------------
// `$` bridge construction.
// ---------------------------------------------------------------------------
// Defined below (after the socket configs); reports the tcp_rx_buf_size of the
// SocketInitConfig actually selected for the current memory regime, so the JS
// socket layer can size its read buffer to match the native configuration
// (1 MiB in application mode, 128 KiB in the lean applet config) instead of
// hard-coding 1 MiB and over-allocating the limited ArrayBuffer pool.
static u32 nx_selected_tcp_rx_buf_size(void);
// The effective SocketInitConfig passed to socketInitialize() (regime base +
// nxjs.ini overrides). Forward-declared so build_init_object() can expose it on
// `$.config.socket`; defined alongside the socket configs below.
static const SocketInitConfig *nx_effective_socket_cfg(void);

static void build_init_object(Isolate *iso, Local<Context> context,
                              Local<Object> init_obj, const char *entrypoint,
                              int argc, char *argv[],
                              const nx_config_t *cfg) {
	nx_init_account(iso, init_obj);
	nx_init_album(iso, init_obj);
	nx_init_applet(iso, init_obj);
	nx_init_audio(iso, init_obj);
	nx_init_battery(iso, init_obj);
	nx_init_bluetooth(iso, init_obj);
	nx_init_canvas(iso, init_obj);
	nx_init_compression(iso, init_obj);
	nx_init_crypto(iso, init_obj);
	nx_init_dns(iso, init_obj);
	nx_init_dommatrix(iso, init_obj);
	nx_init_error(iso, init_obj);
	nx_init_font(iso, init_obj);
	nx_init_fs(iso, init_obj);
	nx_init_fsdev(iso, init_obj);
	nx_init_gamepad(iso, init_obj);
	nx_init_hidsys(iso, init_obj);
	nx_init_image(iso, init_obj);
	nx_init_irs(iso, init_obj);
	nx_init_memory(iso, init_obj);
	nx_init_nifm(iso, init_obj);
	nx_init_ns(iso, init_obj);
	nx_init_path2d(iso, init_obj);
	nx_init_service(iso, init_obj);
	nx_init_swkbd(iso, init_obj);
	nx_init_tcp(iso, init_obj);
	nx_init_tls(iso, init_obj);
	nx_init_udp(iso, init_obj);
	nx_init_url(iso, init_obj);
	nx_init_video(iso, init_obj);
	nx_init_web(iso, init_obj);
	nx_init_webgl(iso, init_obj);
	nx_init_window(iso, init_obj);

	NX_SET_FUNC(init_obj, "exit", js_exit);
	NX_SET_FUNC(init_obj, "queueMicrotask", js_queue_microtask);
	NX_SET_FUNC(init_obj, "cwd", js_cwd);
	NX_SET_FUNC(init_obj, "chdir", js_chdir);
	NX_SET_FUNC(init_obj, "print", js_print);
	NX_SET_FUNC(init_obj, "printErr", js_print_err);
	NX_SET_FUNC(init_obj, "getInternalPromiseState",
	            js_get_internal_promise_state);
	NX_SET_FUNC(init_obj, "getenv", js_getenv);
	NX_SET_FUNC(init_obj, "setenv", js_setenv);
	NX_SET_FUNC(init_obj, "unsetenv", js_unsetenv);
	NX_SET_FUNC(init_obj, "envToObject", js_env_to_object);
	NX_SET_FUNC(init_obj, "onExit", nx_set_exit_handler);
	NX_SET_FUNC(init_obj, "onFrame", nx_set_frame_handler);
	NX_SET_FUNC(init_obj, "framebufferInit", nx_framebuffer_init);
	NX_SET_FUNC(init_obj, "hidInitializeTouchScreen",
	            js_hid_initialize_touch_screen);
	NX_SET_FUNC(init_obj, "hidGetTouchScreenStates",
	            js_hid_get_touch_screen_states);
	NX_SET_FUNC(init_obj, "hidInitializeKeyboard", js_hid_initialize_keyboard);
	NX_SET_FUNC(init_obj, "hidGetKeyboardStates", js_hid_get_keyboard_states);
	NX_SET_FUNC(init_obj, "hidInitializeVibrationDevices",
	            js_hid_initialize_vibration_devices);
	NX_SET_FUNC(init_obj, "hidSendVibrationValues",
	            js_hid_send_vibration_values);

	// Switch.version
	Local<Object> version = Object::New(iso);
	auto set_ver = [&](const char *k, const char *v) {
		version->Set(context, nx_str(iso, k), nx_str(iso, v)).Check();
	};
	set_ver("ada", ADA_VERSION);
	// ams + emummc are lazy (require SPL init); install accessors.
	version
	    ->SetNativeDataProperty(context, nx_str(iso, "ams"),
	                            nx_version_get_ams)
	    .Check();
	version
	    ->SetNativeDataProperty(context, nx_str(iso, "emummc"),
	                            nx_version_get_emummc)
	    .Check();
	{
		char ft[16];
		snprintf(ft, sizeof(ft), "%d.%d.%d", FREETYPE_MAJOR, FREETYPE_MINOR,
		         FREETYPE_PATCH);
		set_ver("freetype2", ft);
	}
	set_ver("harfbuzz", HB_VERSION_STRING);
	char hos[12];
	u32 hv = hosversionGet();
	snprintf(hos, sizeof(hos), "%d.%d.%d", HOSVER_MAJOR(hv), HOSVER_MINOR(hv),
	         HOSVER_MICRO(hv));
	set_ver("hos", hos);
	set_ver("libnx", LIBNX_VERSION);
	set_ver("mbedtls", MBEDTLS_VERSION_STRING);
	set_ver("nxjs", NXJS_VERSION);
	{
		char skia[8];
		snprintf(skia, sizeof(skia), "%d", SK_MILESTONE);
		set_ver("skia", skia);
	}
	set_ver("png", PNG_LIBPNG_VER_STRING);
	set_ver("turbojpeg", LIBTURBOJPEG_VERSION);
	set_ver("v8", V8::GetVersion());
	{
		int wv = WebPGetDecoderVersion();
		char webp[12];
		snprintf(webp, sizeof(webp), "%d.%d.%d", (wv >> 16) & 0xFF,
		         (wv >> 8) & 0xFF, wv & 0xFF);
		set_ver("webp", webp);
	}
	set_ver("zlib", zlibVersion());
	set_ver("zstd", ZSTD_versionString());
	init_obj->Set(context, nx_str(iso, "version"), version).Check();

	// Switch.entrypoint
	init_obj->Set(context, nx_str(iso, "entrypoint"), nx_str(iso, entrypoint))
	    .Check();

	// Switch.argv
	Local<Array> argv_array = Array::New(iso, argc);
	for (int i = 0; i < argc; i++) {
		argv_array->Set(context, i, nx_str_lossy(iso, argv[i])).Check();
	}
	init_obj->Set(context, nx_str(iso, "argv"), argv_array).Check();

	// $.selfNroPath — the source `Application.self` should read its NACP/icon
	// from. This must identify the *app the user launched*, NOT the runtime.
	// The launch mode (mirrors resolve_entrypoint) determines it:
	//   - Standalone (fat) NRO: argv[0] is the app's own NRO -> that path.
	//   - Slim NRO (argv[1] is an app `.nro`): argv[0] is the *runtime* NRO, so
	//     the app is argv[1] -> that path.
	//   - NSP, fat or slim (argv[1] == "nsp:" or no argv[1] for an installed
	//     title): the app is an installed title; null tells nsAppNew to read the
	//     running process's ProgramId via nsGetApplicationControlData.
	//   - Direct `.js` (argv[1] is a non-.nro path): no NACP available; null
	//     (falls back to the running process ProgramId).
	// Without this, Application.self keyed off argv[0] and resolved to the
	// shared runtime NRO ("nx.js") in both slim modes.
	Local<Value> self_nro_path = Null(iso);
	if (argc > 1 && argv[1] && argv[1][0]) {
		if (strcmp(argv[1], "nsp:") != 0 && path_has_suffix(argv[1], ".nro")) {
			self_nro_path = nx_str_lossy(iso, argv[1]);
		}
	} else if (argc > 0 && argv[0] && argv[0][0]) {
		self_nro_path = nx_str_lossy(iso, argv[0]);
	}
	init_obj->Set(context, nx_str(iso, "selfNroPath"), self_nro_path).Check();

	// The configured bsdsocket TCP receive buffer size (regime-dependent). The
	// JS socket layer sizes its per-socket read buffer to this instead of a
	// hard-coded 1 MiB, so applet mode (128 KiB) does not over-allocate.
	init_obj
	    ->Set(context, nx_str(iso, "tcpRxBufSize"),
	          Integer::NewFromUnsigned(iso, nx_selected_tcp_rx_buf_size()))
	    .Check();

	// Switch-side config (`$.config`): the EFFECTIVE values applied from
	// nxjs.ini (after clamping/regime defaults), for diagnostics. Mirrors the
	// `version` object pattern above.
	{
		Local<Object> conf = Object::New(iso);
		auto cset = [&](const char *k, Local<Value> v) {
			conf->Set(context, nx_str(iso, k), v).Check();
		};
		cset("jit", Boolean::New(iso, cfg->effective_jit));
		cset("heapLimit",
		     Number::New(iso, (double)cfg->effective_heap_limit));
		cset("codeHeadroomMb",
		     Integer::NewFromUnsigned(iso, cfg->effective_code_headroom_mb));
		const char *rmode = cfg->renderer == NX_RENDER_CPU   ? "cpu"
		                    : cfg->renderer == NX_RENDER_GPU ? "gpu"
		                                                     : "auto";
		cset("renderer", nx_str(iso, rmode));
		cset("v8Flags",
		     nx_str_lossy(iso, cfg->v8_flags ? cfg->v8_flags : ""));

		const SocketInitConfig *esc = nx_effective_socket_cfg();
		Local<Object> sock = Object::New(iso);
		auto sset = [&](const char *k, u32 v) {
			sock->Set(context, nx_str(iso, k),
			          Integer::NewFromUnsigned(iso, v))
			    .Check();
		};
		sset("tcpTxBufSize", esc->tcp_tx_buf_size);
		sset("tcpRxBufSize", esc->tcp_rx_buf_size);
		sset("tcpTxBufMaxSize", esc->tcp_tx_buf_max_size);
		sset("tcpRxBufMaxSize", esc->tcp_rx_buf_max_size);
		sset("udpTxBufSize", esc->udp_tx_buf_size);
		sset("udpRxBufSize", esc->udp_rx_buf_size);
		sset("sbEfficiency", esc->sb_efficiency);
		sset("numBsdSessions", esc->num_bsd_sessions);
		sset("serviceType", (u32)esc->bsd_service_type);
		conf->Set(context, nx_str(iso, "socket"), sock).Check();

		// `$.config.console`: the `[console]` styling from nxjs.ini, passed
		// through verbatim (only keys that were set). The JS console layer
		// seeds the global console's options from this (an explicit
		// `console.options =` assignment overrides). A `theme` sub-object holds
		// the colors so it maps directly onto the TerminalOptions theme shape.
		{
			const nx_console_config_t *cc = &cfg->console;
			Local<Object> con = Object::New(iso);
			auto cnset = [&](const char *k, Local<Value> v) {
				con->Set(context, nx_str(iso, k), v).Check();
			};
			if (cc->has_font_size)
				cnset("fontSize", Number::New(iso, cc->font_size));
			if (cc->has_line_height)
				cnset("lineHeight", Number::New(iso, cc->line_height));
			if (cc->has_scrollback)
				cnset("scrollback",
				      Integer::NewFromUnsigned(iso, cc->scrollback));
			if (cc->cursor_style != NX_CURSOR_UNSET) {
				const char *cs = cc->cursor_style == NX_CURSOR_UNDERLINE
				                     ? "underline"
				                 : cc->cursor_style == NX_CURSOR_BAR ? "bar"
				                                                     : "block";
				cnset("cursorStyle", nx_str(iso, cs));
			}
			if (cc->has_cursor_opacity)
				cnset("cursorOpacity", Number::New(iso, cc->cursor_opacity));

			// Theme colors -> a `theme` object (only the set ones).
			static const char *const THEME_KEYS[16] = {
			    "black",       "red",        "green",       "yellow",
			    "blue",        "magenta",    "cyan",        "white",
			    "brightBlack", "brightRed",  "brightGreen", "brightYellow",
			    "brightBlue",  "brightMagenta", "brightCyan", "brightWhite"};
			Local<Object> theme = Object::New(iso);
			bool any_theme = false;
			auto tset = [&](const char *k, const char *v) {
				if (!v)
					return;
				theme->Set(context, nx_str(iso, k), nx_str_lossy(iso, v))
				    .Check();
				any_theme = true;
			};
			tset("background", cc->background);
			tset("foreground", cc->foreground);
			tset("cursor", cc->cursor);
			for (int i = 0; i < 16; i++)
				tset(THEME_KEYS[i], cc->ansi[i]);
			if (any_theme)
				cnset("theme", theme);

			conf->Set(context, nx_str(iso, "console"), con).Check();
		}

		conf->Set(context, nx_str(iso, "loaded"),
		          Boolean::New(iso, cfg->loaded))
		    .Check();
		init_obj->Set(context, nx_str(iso, "config"), conf).Check();
	}
}

// BsdServiceType_Auto (tries bsd:s first) is intentional: bsd:s is required to
// bind privileged ports (e.g. a TCP server on port 80).
//
// The socket layer reserves transfer memory per socket sized roughly as
// (tcp_tx_buf_size + tcp_rx_buf_size) * sb_efficiency, drawn from a shared pool
// of ~(tcp_tx_max + tcp_rx_max + udp_tx + udp_rx) * sb_efficiency. A previous
// config used 1 MiB tx/rx buffers with sb_efficiency=8, reserving ~16 MiB PER
// socket — so only ~4 sockets could exist concurrently before connect() failed
// with ENOBUFS ("No buffer space available"). That breaks any concurrent
// fetch / redirect-follow workload. Use a moderate per-socket footprint
// (256 KiB initial, autotuning up to 1 MiB) so 20+ sockets coexist while
// preserving high single-stream throughput. The lean config (below) is used in
// applet mode (~137 MiB total) where a large reservation starves Mesa/V8 and
// bsdsocket faults on libuv's loopback self-wake socketpair (User Break at
// bsdsocket+0xe7064).
static SocketInitConfig const s_socketInitConfigFull = {
    .tcp_tx_buf_size = 256 * 1024,
    .tcp_rx_buf_size = 256 * 1024,
    .tcp_tx_buf_max_size = 1 * 1024 * 1024,
    .tcp_rx_buf_max_size = 1 * 1024 * 1024,
    .udp_tx_buf_size = 0x2400,
    .udp_rx_buf_size = 0xA500,
    .sb_efficiency = 6,
    .num_bsd_sessions = 3,
    .bsd_service_type = BsdServiceType_Auto,
};

// Lean config for tight-memory (applet) mode. The previous lean config used
// 128 KiB initial / 256 KiB max buffers with sb_efficiency=4, which reserved
// ~1 MiB PER socket and a total pool of only ~2 MiB — barely two concurrent
// sockets, so a redirect-following fetch (whose two hops briefly overlap during
// the socket handoff) failed the second connect() with ENOBUFS. Shrink the
// per-socket *initial* buffers (64 KiB, matching libnx's default rx size) so
// each socket's footprint is small, while raising sb_efficiency to grow the
// shared pool enough for ~15+ concurrent sockets. Total reservation is roughly
// (tx_max + rx_max + udp_tx + udp_rx) * sb_efficiency ~= 512 KiB * 8 ~= 4 MiB,
// still small enough to leave room for the GPU/Mesa + V8 stack in applet mode
// (~137 MiB total).
static SocketInitConfig const s_socketInitConfigLean = {
    .tcp_tx_buf_size = 64 * 1024,
    .tcp_rx_buf_size = 64 * 1024,
    .tcp_tx_buf_max_size = 256 * 1024,
    .tcp_rx_buf_max_size = 256 * 1024,
    .udp_tx_buf_size = 0x2400,
    .udp_rx_buf_size = 0xA500,
    .sb_efficiency = 8,
    .num_bsd_sessions = 3,
    .bsd_service_type = BsdServiceType_Auto,
};

// The SocketInitConfig actually passed to socketInitialize() — the
// regime-selected base after any [socket] nxjs.ini overrides + clamping. Set
// once in main() before socketInitialize. Initialized to the full config so a
// read before assignment (shouldn't happen) is still sane.
static SocketInitConfig g_effective_socket_cfg = s_socketInitConfigFull;

// tcp_rx_buf_size of the config socketInitialize() actually used (so the JS
// socket layer sizes its read buffer to match the native config).
static u32 nx_selected_tcp_rx_buf_size(void) {
	return g_effective_socket_cfg.tcp_rx_buf_size;
}

static const SocketInitConfig *nx_effective_socket_cfg(void) {
	return &g_effective_socket_cfg;
}

// Resolve the user entrypoint and (for standalone / bootstrap-.nro launches)
// mount `romfs:`. Hoisted out of main()'s body so it can run BEFORE V8 init:
// the entrypoint directory is needed to locate `nxjs.ini`, whose [v8]/[memory]
// settings must be applied before the isolate is created. Two launch models:
//
//   1. Bootstrap launch: `argv[1]` is an explicit entrypoint passed by a thin
//      launcher. Either a `.nro` (mount its embedded romfs as `romfs:`, run
//      `romfs:/main.js`) or any other path (typically `.js`: run it directly).
//   2. Standalone app (no argv[1]): the app's own NRO romfs holds `main.js` +
//      assets, so mount self as `romfs:` and run `romfs:/main.js`; fall back to
//      `<nro>.js` next to the NRO on the SD card.
static void resolve_entrypoint(nx_context_t *nx_ctx, int argc, char *argv[],
                               char **out_path, char **out_code,
                               size_t *out_size, bool *out_needs_free) {
	*out_path = NULL;
	*out_code = NULL;
	*out_size = 0;
	*out_needs_free = false;
	Result rc;

	if (argc > 1 && argv[1] && argv[1][0]) {
		// Bootstrap launch with an explicit entrypoint.
		if (strcmp(argv[1], "nsp:") == 0) {
			// Slim NSP launch: a forwarder hbloader (the title's `main` NSO,
			// see bootstrap/launcher-nsp) loaded us, the runtime NRO, with the
			// "nsp:" marker. The app's `main.js` + assets live in the *installed
			// title's* RomFS, which is this process's data storage — mount it
			// via romfsMountFromCurrentProcess (fs cmd 200), independent of the
			// NRO/NSO ABI. (mount_nro_romfs / romfsMountSelf wouldn't work here:
			// our own argv[0] is the runtime NRO, whose RomFS is `nxjs:`.)
			rc = romfsMountFromCurrentProcess("romfs");
			if (R_FAILED(rc)) {
				nx_console_init(nx_ctx);
				printf("Failed to mount the installed title's romfs (0x%x)\n",
				       rc);
				nx_ctx->had_error = 1;
			} else {
				*out_path = (char *)"romfs:/main.js";
				*out_code = (char *)read_file(*out_path, out_size);
			}
		} else if (path_has_suffix(argv[1], ".nro")) {
			rc = mount_nro_romfs(argv[1], "romfs");
			if (R_FAILED(rc)) {
				nx_console_init(nx_ctx);
				printf("Failed to mount romfs from %s (0x%x)\n", argv[1], rc);
				nx_ctx->had_error = 1;
			} else {
				*out_path = (char *)"romfs:/main.js";
				*out_code = (char *)read_file(*out_path, out_size);
			}
		} else {
			*out_needs_free = true;
			*out_path = strdup(argv[1]);
			if (*out_path) {
				*out_code = (char *)read_file(*out_path, out_size);
			}
		}
	} else {
		// Standalone app: this NRO's own romfs is the app. Also expose it as
		// `romfs:` (it is already mounted as `nxjs:` for the runtime's files).
		// romfsMountSelf opens our NRO file again with an independent handle, so
		// the second mount coexists with the `nxjs:` one.
		rc = romfsMountSelf("romfs");
		if (R_SUCCEEDED(rc)) {
			*out_path = (char *)"romfs:/main.js";
			*out_code = (char *)read_file(*out_path, out_size);
		}
		if (*out_code == NULL && errno == ENOENT && argc > 0) {
			*out_needs_free = true;
			*out_path = strdup(argv[0]);
			if (*out_path) {
				replace_file_extension(*out_path, ".js");
				*out_code = (char *)read_file(*out_path, out_size);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char *argv[]) {
	Result rc;

	nx_context_t *nx_ctx = (nx_context_t *)calloc(1, sizeof(nx_context_t));
	nx_ctx->rendering_mode = NX_RENDERING_MODE_INIT;

	// Mount the nx.js NRO's OWN embedded romfs as `nxjs:` (it holds the
	// runtime's source map). The app's romfs is mounted separately as `romfs:`
	// during entrypoint resolution below: for a standalone app that is also
	// this NRO's romfs; for a bootstrap launch (argv[1] = an app `.nro`) it is
	// the launched app's romfs. Keeping the runtime files under `nxjs:` lets
	// `romfs:` always mean "the app".
	//
	// Use romfsMountSelf (NRO-aware: reads our own NRO file via argv[0]), NOT
	// romfsMountFromCurrentProcess (which uses fsOpenDataStorageByCurrentProcess
	// — that is for installed titles/NSO and fails for a homebrew NRO).
	rc = romfsMountSelf("nxjs");
	if (R_FAILED(rc))
		diagAbortWithResult(rc);

	// Probe the process memory grant up front: it gates BOTH the socket buffer
	// config (here) and the V8 JIT/heap policy (below). In applet mode the lean
	// socket config avoids a ~64 MiB transfer-memory reservation that would
	// otherwise starve the GPU/V8 stack and fault bsdsocket.
	//
	// IMPORTANT: gate on InfoType_TotalMemorySize (the size of the memory grant
	// for the process), NOT (total - used). Applet mode grants a small total
	// (~512 MiB); application mode grants several GiB. "total - used" looks like
	// only a few MiB free in application mode (the large grant is reported as
	// reserved/used), which would wrongly pick the tight-memory path and run
	// jitless even though there is plenty of room for full JIT + GPU.
	u64 mem_total = 0, mem_used = 0;
	svcGetInfo(&mem_total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
	svcGetInfo(&mem_used, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE, 0);
	u64 mem_free = mem_total > mem_used ? mem_total - mem_used : 0;
	// >1 GiB total => application/full-memory regime (full JIT + GPU coexist).
	// <=1 GiB total => applet regime (~512 MiB grant): jitless + lean sockets.
	bool tight_memory = mem_total <= (1024ull * 1024 * 1024);
	g_tight_memory = tight_memory;

	// Redirect stderr to the on-SD debug log NOW (before socket init and the
	// config parse) so that `[config]`/`[v8]`/`[skia]` diagnostics — including
	// any "value not honored" lines from the nxjs.ini parse below — are
	// captured to sdmc:/switch/nxjs-debug.log.
	FILE *debug_fd = freopen(LOG_FILENAME, "w", stderr);

	// Take a process-lifetime nvdrv reference NOW, while memory is plentiful.
	// The libnx PrintConsole/Framebuffer stack nvInitialize()/nvExit()s around
	// each framebuffer, so without this reference the boot console's teardown
	// fully closes nvdrv and the later canvas-mode framebufferCreate() must
	// re-open it — and that close→reopen of nvdrv:a was observed (on-device)
	// to hang inside nvservices' Initialize when applet memory is nearly
	// exhausted. Holding the session from boot makes those inner init/exit
	// calls pure refcount operations. Released at teardown below.
	{
		Result nvrc = nvInitialize();
		if (R_SUCCEEDED(nvrc)) {
			g_nv_early_ref = true;
		} else {
			fprintf(stderr, "[fb] early nvInitialize failed: 0x%x\n", nvrc);
			fflush(stderr);
		}
	}

	// (The applet-regime display parachute is reserved AFTER runtime.js
	// evaluation below — reserving it here, before V8 commits its boot
	// footprint, was observed to push V8's own heap growth into a fatal OOM.)

	// Resolve the app entrypoint up front (this is the hoisted version of the
	// block that used to run after V8 init). We need the entrypoint path early
	// because (a) `nxjs.ini` lives next to it, and (b) the V8 flag/heap/JIT
	// settings it can override MUST be applied before V8::Initialize() below.
	// This mounts `romfs:` (for standalone / bootstrap-.nro launches) and reads
	// the user code. The actual evaluation still happens much later.
	size_t user_code_size = 0;
	bool user_path_needs_free = false;
	char *user_code_path = NULL;
	char *user_code = NULL;
	resolve_entrypoint(nx_ctx, argc, argv, &user_code_path, &user_code,
	                   &user_code_size, &user_path_needs_free);

	// Parse `nxjs.ini` (next to the entrypoint) into the per-isolate config.
	// Missing/unreadable file -> defaults, silently. Done before socket init +
	// V8 init so every setting can take effect.
	nx_config_defaults(&nx_ctx->config);
	if (user_code_path) {
		char *ini_path = nx_config_ini_path_for(user_code_path);
		if (ini_path) {
			nx_config_load(&nx_ctx->config, ini_path);
			free(ini_path);
		}
	}

	// Socket buffers: start from the regime-selected base, then apply any
	// [socket] overrides from nxjs.ini (clamped + logged).
	SocketInitConfig socket_cfg =
	    tight_memory ? s_socketInitConfigLean : s_socketInitConfigFull;
	nx_config_apply_socket(&socket_cfg, &nx_ctx->config, tight_memory);
	g_effective_socket_cfg = socket_cfg;
	rc = socketInitialize(&socket_cfg);
	if (R_FAILED(rc))
		diagAbortWithResult(rc);

	rc = plInitialize(PlServiceType_User);
	if (R_FAILED(rc))
		diagAbortWithResult(rc);

	// libuv loop.
	uv_loop_t loop;
	uv_loop_init(&loop);
	nx_ctx->loop = &loop;

	// V8 platform + isolate (single-threaded, matching the switch-v8 port).
	// NewSingleThreadedDefaultPlatform: the multi-threaded default spins worker
	// threads whose abseil/pthread sync faults on Horizon.
	std::unique_ptr<Platform> platform =
	    platform::NewSingleThreadedDefaultPlatform();
	V8::InitializePlatform(platform.get());

	// Memory-gated JIT policy: full-JIT's libnx jitCreate dual-maps a >=64 MiB
	// code range (~128 MiB real). In applet mode (~137 MiB free) that starves
	// the GPU/Mesa stack, so by default we fall back to jitless (interpreter) +
	// raster canvas when free memory is tight; application mode (~3 GiB) runs
	// full JIT + GPU. mem_free / tight_memory were probed before
	// socketInitialize above. ~300 MiB headroom: code arena (128 MiB real) +
	// GPU/Mesa + JS heap.
	//
	// nxjs.ini [v8] jit overrides this: `auto` keeps the regime default; `on`
	// forces full JIT; `off` forces jitless. JIT is honored verbatim (no clamp)
	// — JIT in applet mode is fine for CPU rendering; only the applet + GPU +
	// JIT combination is known to crash (jitCreate starves Mesa), for which we
	// emit a warning below but still proceed.
	bool can_jit;
	switch (nx_ctx->config.jit) {
	case NX_JIT_ON:
		can_jit = true;
		break;
	case NX_JIT_OFF:
		can_jit = false;
		break;
	case NX_JIT_AUTO:
	default:
		// Full JIT in BOTH memory regimes by default. This is safe now that the
		// JIT code-arena headroom is regime-gated (applet mode reserves 0 WASM
		// headroom -> the 64 MiB code-range minimum, which fits): verified
		// across the example apps on-device in applet mode (canvas, snake, svg,
		// fonts, react, audio, http/websocket servers, repl — all stable with
		// clean exit). Applet mode still defaults to CPU raster rendering
		// (chosen independently of JIT; an explicit `[renderer] gpu` can opt in
		// to GPU even in applet — the known-unstable combo handled below) and
		// keeps WASM opt-in (no code headroom by default; see the code-arena
		// budget below). Apps can force the jitless interpreter with
		// `[v8] jit = off`.
		can_jit = true;
		break;
	}
	nx_ctx->config.effective_jit = can_jit;
	if (nx_ctx->config.jit == NX_JIT_ON && tight_memory &&
	    nx_ctx->config.renderer == NX_RENDER_GPU) {
		fprintf(stderr,
		        "[config] jit=on + renderer=gpu in applet regime: known-"
		        "unstable (V8 jitCreate starves Mesa); may crash\n");
		fflush(stderr);
	}

	// JIT code-arena budget. The libnx jit_* code arena is dual-mapped (rx+rw),
	// so it costs ~2x its size in real memory: V8's 64 MiB code-range floor is
	// ~128 MiB real, and the default 64 MiB WASM headroom on top would make it
	// ~256 MiB — impossible in applet mode (~137 MiB free). That oversized arena
	// was the root cause of applet+JIT failures (early bsdsocket crash for apps
	// that didn't fit; a ~5s render-loop freeze as the heap gradually exhausted).
	//
	// So pick the WASM headroom by regime unless the app overrides it:
	//   - application mode: 64 MiB (WASM works out of the box).
	//   - applet mode: 0 (WASM is opt-in via [v8] code_headroom_mb / wasm=on;
	//     applet can't afford the dual-mapped headroom by default).
	// 0 headroom => 64 MiB arena (V8's minimum code range; cannot go lower).
	uint32_t headroom_mb = 0;
	if (can_jit) {
		headroom_mb = nx_ctx->config.code_headroom_mb;
		if (headroom_mb == NX_CODE_HEADROOM_AUTO)
			headroom_mb = tight_memory ? 0u : 64u;
		horizon_mman_set_code_budget(headroom_mb, 0);
		fprintf(stderr, "[v8] code arena: WASM headroom = %u MiB%s\n",
		        headroom_mb,
		        nx_ctx->config.code_headroom_mb == NX_CODE_HEADROOM_AUTO
		            ? " (regime default)"
		            : " (config)");
		fflush(stderr);
	}
	nx_ctx->config.effective_code_headroom_mb = headroom_mb;

	if (can_jit) {
		V8::SetFlagsFromString("--single-threaded --single-threaded-gc "
		                       "--predictable");
	} else {
		V8::SetFlagsFromString("--jitless --single-threaded "
		                       "--single-threaded-gc "
		                       "--no-concurrent-recompilation --predictable");
	}
	// App-provided V8 flags, applied AFTER the runtime defaults so they take
	// precedence (V8's later SetFlagsFromString wins). Advanced/at-own-risk:
	// V8 silently ignores unknown flags, so we just record what was applied.
	if (nx_ctx->config.v8_flags && nx_ctx->config.v8_flags[0]) {
		fprintf(stderr, "[config] applying app V8 flags: \"%s\"\n",
		        nx_ctx->config.v8_flags);
		V8::SetFlagsFromString(nx_ctx->config.v8_flags);
	}
	// Report the memory gate + JIT mode. When JIT is on, the linked monolith
	// tiers up Ignition -> Sparkplug -> Maglev -> TurboFan; jitless is Ignition
	// only.
	fprintf(stderr,
	        "[v8] mem_total=%llu MiB free=%llu MiB regime=%s -> mode=%s (%s)%s\n",
	        (unsigned long long)(mem_total / (1024 * 1024)),
	        (unsigned long long)(mem_free / (1024 * 1024)),
	        tight_memory ? "applet" : "application",
	        can_jit ? "jit" : "jitless",
	        can_jit ? "Ignition+Sparkplug+Maglev+TurboFan" : "Ignition only",
	        nx_ctx->config.jit != NX_JIT_AUTO ? " [config override]" : "");
	// stderr is fully buffered when redirected to a file (not a TTY), so an
	// early abnormal exit would lose this line. Flush each init milestone so the
	// log reflects how far startup actually got.
	fflush(stderr);
	V8::Initialize();

	Isolate::CreateParams create_params;
	create_params.array_buffer_allocator =
	    ArrayBuffer::Allocator::NewDefaultAllocator();

	// Size V8's heap for the device. Without this V8 uses desktop defaults
	// (assuming GBs of RAM) and only discovers it's out of memory by fatally
	// aborting ("JavaScript OOM: CALL_AND_RETRY_LAST") instead of GCing harder.
	//
	// IMPORTANT: use ConfigureDefaultsFromHeapSize(initial, max) (explicit heap
	// bounds), NOT ConfigureDefaults(physical_memory, ...) — the latter derives
	// a large semi-space + heap from the "physical memory" figure, which makes
	// V8 reserve/commit oversized arenas that the horizon mman can't satisfy
	// (crash in Arena::CommitRange -> _malloc_r). This matches the hello-v8
	// reference. ConfigureDefaultsFromHeapSize also RESETS the code range size,
	// so (re)apply it AFTER.
	//
	// Budget the V8 object heap per memory regime. Two facts drive this:
	//   - horizon_mman_data_arena_size(): the ADDRESS SPACE switch-v8 reserved
	//     (from the STACK region) for the V8 heap (~1 GiB), committed lazily in
	//     16 MiB slabs as V8 touches pages. This is an address-space ceiling,
	//     NOT a guarantee of physical RAM.
	//   - the actual physical memory the process can commit.
	// In APPLICATION mode the grant is several GiB but mem_free (= total - used)
	// is misleading: Horizon reports the whole grant as "used" at startup, so
	// mem_free reads as only a few MiB. There, size from the arena ceiling
	// (capped) — physical RAM is plentiful. In APPLET mode the grant is small
	// (~380 MiB) and mem_free IS meaningful (~137 MiB free); the 1 GiB arena is
	// pure address space the device cannot back, so configuring a big heap makes
	// V8 grow until physical commits fail and it fatally OOMs
	// ("CALL_AND_RETRY_LAST"). So in applet mode clamp to real free RAM.
	// Reserve headroom (outside the V8 managed heap) for the JIT code arena,
	// GPU/Mesa, libuv, and native allocs — they share the process memory grant.
	// The size depends on the JIT mode, the regime, and (in applet) whether GPU
	// was explicitly requested:
	//   - application + JIT: GPU/Mesa is the big consumer; reserve 180 MiB out
	//     of the ~1 GiB address-space arena (heap then caps at 512 MiB below).
	//   - applet + JIT, default (raster): no GPU, so the only large reservation
	//     is the 64 MiB JIT code range (which libnx jitCreate dual-maps) plus
	//     libuv / Skia raster / native. ~64 MiB leaves a usable heap from the
	//     ~137 MiB applet grant. (The previous 180 MiB here was an
	//     application-mode figure: it underflowed applet's free RAM to 0 and
	//     collapsed the heap to the 32 MiB floor, which is too small for the
	//     JIT/Wasm compiler and OOM'd — even though full JIT + a ~96 MiB heap is
	//     known to work in applet mode. See the trifecta CPU example.)
	//   - applet + JIT + explicit `[renderer] gpu`: GPU/Mesa IS active (this is
	//     the known-unstable combo warned about above). Use the larger 180 MiB
	//     reserve so the V8 heap stays small and leaves Mesa as much room as
	//     possible — the smaller raster reserve would grow the heap and starve
	//     Mesa further. (`mem_free` - 180 underflows to the 32 MiB heap floor,
	//     which is the right outcome here: minimal heap, maximal Mesa headroom.)
	//   - jitless (either regime): no code range -> 48 MiB.
	bool applet_gpu = tight_memory &&
	                  nx_ctx->config.renderer == NX_RENDER_GPU;
	u64 reserve;
	if (!can_jit)
		reserve = 48ull * 1024 * 1024;
	else if (tight_memory && !applet_gpu)
		reserve = 64ull * 1024 * 1024;
	else
		reserve = 180ull * 1024 * 1024;
	{
		u64 arena = (u64)horizon_mman_data_arena_size();
		u64 ceiling;
		if (tight_memory) {
			// Applet: bounded by real free physical RAM, not the address-space
			// arena. mem_free is reliable in this regime.
			ceiling = mem_free;
		} else {
			// Application: physical RAM is plentiful; mem_free is unreliable.
			// Size from the mman's reserved arena (normally ~1 GiB -> capped at
			// 512 MiB below). If the arena reservation reported 0 (it failed),
			// fall back to 192 MiB of address space; after the `reserve`
			// subtraction this lands on the 32 MiB floor — a deliberately
			// conservative last resort, since with no arena we can't trust a
			// larger heap to be backable.
			ceiling = arena ? arena : 192ull * 1024 * 1024;
		}
		u64 computed = ceiling > reserve ? ceiling - reserve : 0;
		u64 max_heap = computed;
		// nxjs.ini [memory] heap_limit override. Allowed to raise or lower the
		// limit, but still clamped to what is physically backable (the same
		// ceiling - reserve the runtime computed) so a too-high value can't
		// guarantee a FatalOOM. The 32 MiB floor / 512 MiB cap below also apply.
		bool heap_overridden = false;
		if (nx_ctx->config.heap_limit != 0) {
			u64 req = nx_ctx->config.heap_limit;
			max_heap = req;
			if (max_heap > computed) {
				fprintf(stderr,
				        "[config] memory.heap_limit=%llu MiB not honored: "
				        "exceeds backable %llu MiB (ceiling %llu - reserve %llu "
				        "MiB), clamped\n",
				        (unsigned long long)(req / (1024 * 1024)),
				        (unsigned long long)(computed / (1024 * 1024)),
				        (unsigned long long)(ceiling / (1024 * 1024)),
				        (unsigned long long)(reserve / (1024 * 1024)));
				max_heap = computed;
			}
			heap_overridden = true;
		}
		if (max_heap < 32ull * 1024 * 1024) {
			if (heap_overridden)
				fprintf(stderr,
				        "[config] memory.heap_limit not honored: below 32 MiB "
				        "floor, clamped to 32 MiB\n");
			max_heap = 32ull * 1024 * 1024;
		}
		if (max_heap > 512ull * 1024 * 1024) {
			if (heap_overridden)
				fprintf(stderr,
				        "[config] memory.heap_limit not honored: above 512 MiB "
				        "cap, clamped to 512 MiB\n");
			max_heap = 512ull * 1024 * 1024;
		}
		nx_ctx->config.effective_heap_limit = max_heap;
		create_params.constraints.ConfigureDefaultsFromHeapSize(
		    8ull * 1024 * 1024 /* initial */, max_heap /* max */);
		fprintf(stderr,
		        "[v8] max_heap=%llu MiB (arena=%llu MiB free=%llu MiB)%s\n",
		        (unsigned long long)(max_heap / (1024 * 1024)),
		        (unsigned long long)(arena / (1024 * 1024)),
		        (unsigned long long)(mem_free / (1024 * 1024)),
		        heap_overridden ? " [config override]" : "");
		// stderr is fully buffered when redirected to a file; flush so this
		// milestone survives an early abnormal exit (matches the other init
		// logs above).
		fflush(stderr);
	}

	if (can_jit) {
		// Pin the JIT code range to the 64 MiB minimum (V8 defaults to 256 MiB,
		// which libnx jitCreate double-maps and starves the data arena).
		create_params.constraints.set_code_range_size_in_bytes(64ull * 1024 *
		                                                        1024);
	} else {
		// Jitless: no code range -> no jitCreate -> frees ~128 MiB.
		create_params.constraints.set_code_range_size_in_bytes(0);
	}
	Isolate *iso = Isolate::New(create_params);
	nx_ctx->iso = iso;
	iso->SetData(0, nx_ctx);
	iso->SetPromiseRejectCallback(nx_promise_rejection_handler);
	// ES module loading (import.meta + static/dynamic import); see module.cc.
	nx_init_modules(iso);
	// Microtasks are pumped explicitly from the loop.
	iso->SetMicrotasksPolicy(MicrotasksPolicy::kExplicit);

	padConfigureInput(8, HidNpadStyleSet_NpadStandard | HidNpadStyleTag_NpadGc);
	padInitializeDefault(&nx_ctx->pads[0]);
	for (int i = 1; i < 8; i++) {
		padInitialize(&nx_ctx->pads[i],
		              (HidNpadIdType)(HidNpadIdType_No1 + i));
	}

	// `hid:sys` for hardware-backed Gamepad.id (serial numbers) + the
	// controller connect/disconnect event. Non-fatal: on failure, Gamepad.id
	// falls back to the index-based string and connection events stop firing.
	nx_hidsys_init(iso);

	// The user entrypoint (user_code_path / user_code) was already resolved
	// near the top of main() — before V8 init — so that `nxjs.ini` (which sits
	// next to it) could be read in time to influence the V8 flag/heap/JIT
	// settings. See resolve_entrypoint() and its call site above.

	{
		Isolate::Scope iso_scope(iso);
		HandleScope handle_scope(iso);
		Local<Context> context = Context::New(iso);
		Context::Scope context_scope(context);
		Local<Object> global = context->Global();

		if (user_code == NULL) {
			// A more specific error may already have been reported (e.g. an
			// argv[1] `.nro` whose romfs failed to mount, leaving had_error set
			// and user_code_path NULL). Only print the generic errno message
			// when we actually have a path that couldn't be read.
			if (!nx_ctx->had_error) {
				nx_console_init(nx_ctx);
				printf("%s: %s\n", strerror(errno),
				       user_code_path ? user_code_path : "(no entrypoint)");
				nx_ctx->had_error = 1;
			}
		} else {
			Local<Object> init_obj = Object::New(iso);
			nx_ctx->init_obj.Reset(iso, init_obj);
			build_init_object(iso, context, init_obj, user_code_path, argc,
			                  argv, &nx_ctx->config);
			global->Set(context, nx_str(iso, "$"), init_obj).Check();

			// Initialize the runtime (embedded runtime.js source). Name it
			// `nxjs:/runtime.js` so its stack frames symbolicate against the
			// source map at `nxjs:/runtime.js.map` (the runtime's own romfs,
			// mounted as `nxjs:`).
			//
			// Initialize the libnx PrintConsole *before* running runtime.js so
			// that if runtime.js throws during evaluation, print_js_error()'s
			// stdout output (the actual exception + stack) lands on screen
			// instead of being swallowed. Otherwise the user would only ever
			// see a generic "Runtime initialization failed" with no detail.
			nx_console_init(nx_ctx);
			if (!run_script(iso, context,
			                (const char *)nxjs_runtime_js,
			                nxjs_runtime_js_len, "nxjs:/runtime.js")) {
				// run_script already printed the underlying error (exception +
				// stack) via print_js_error(). Add a trailing hint; the full
				// error is also in sdmc:/switch/nxjs-debug.log.
				printf("\nRuntime initialization failed (see error above).\n");
				nx_ctx->had_error = 1;
			} else {
				// Applet regime: pre-fund the display path. The raster present
				// needs ~11.2 MiB at framebufferInit time (2x ~3.7 MiB
				// block-linear swapchain buffers + ~3.7 MiB linear staging),
				// but applet + JIT leaves only a few MiB of slack once V8 +
				// the runtime are up — a failed (or worse, hung) framebuffer
				// bring-up means a black screen. Reserve the memory NOW —
				// after runtime.js evaluation (so V8's boot-time heap growth
				// isn't squeezed into a fatal OOM) but before user code can
				// consume the remaining slack — and release it right before
				// framebufferCreate() (see nx_framebuffer_init), so display
				// bring-up can never lose the race against the JS heap.
				if (g_tight_memory) {
					g_display_parachute =
					    memalign(0x1000, 12ull * 1024 * 1024);
					if (g_display_parachute == NULL) {
						fprintf(stderr,
						        "[fb] display parachute reservation failed\n");
						fflush(stderr);
					}
				}
				// Run the user entrypoint as an ES module.
				nx_run_entry_module(iso, context, user_code, user_code_size,
				                    user_code_path);
			}
		}

		if (user_code) {
			free(user_code);
		}
		if (user_path_needs_free) {
			free(user_code_path);
		}

		// ---- Main event loop ---------------------------------------------
		// Gate on is_running (set false by Switch.exit / + / exit syscall), NOT
		// on appletMainLoop(): once EGL owns the default NWindow (GPU canvas),
		// appletMainLoop() can return false immediately and tear the applet down
		// at frame 1 before any clean exit -> leaked V8 svcMapMemory arenas ->
		// corrupted hbmenu on return. appletMainLoop() is still pumped inside the
		// loop for applet message processing; its return value is only honored as
		// an exit signal when NOT driving a GPU surface (legacy framebuffer apps
		// rely on it to detect an OS-initiated close).
		while (is_running) {
			bool applet_active = appletMainLoop();
			// When the screen is GPU-backed, EGL owns the NWindow and
			// appletMainLoop() can return false immediately, which would end the
			// loop at frame 1 before a clean exit (leaking V8's svcMapMemory
			// arenas and corrupting the next launch). So only honor its false
			// return as an exit signal on the raster framebuffer path; GPU mode
			// exits via + / Switch.exit() (handled below).
			if (!screen_is_gpu && !nx_webgl_active() && !applet_active)
				break;
			if (!nx_ctx->had_error) {
				// libuv: sockets, fs, dns, threadpool afters, timers.
				uv_run(&loop, UV_RUN_NOWAIT);
				// Drain V8 microtasks (promise reactions).
				iso->PerformMicrotaskCheckpoint();
				// Surface any unhandled rejection collected this turn.
				if (!nx_ctx->unhandled_rejected_promise.IsEmpty()) {
					nx_emit_unhandled_rejection_event(iso);
				}
			}

			for (int i = 0; i < 8; i++) {
				padUpdate(&nx_ctx->pads[i]);
			}
			u64 kDown = padGetButtonsDown(&nx_ctx->pads[0]);
			bool plusDown = kDown & HidNpadButton_Plus;

			// `+` is handled by the JS frame handler below ($.onFrame dispatches
			// a cancelable "beforeunload" and calls $.exit() unless prevented),
			// exactly as on the raster path. Do NOT short-circuit it here: a
			// native break would bypass preventDefault() (e.g. snake pausing on
			// + during gameplay instead of exiting). The no-frame-handler case
			// still exits on + via the branch further down.

			// Script called Switch.exit(): break from any state so we always
			// reach teardown (incl. horizon_mman_teardown()).
			if (!is_running)
				break;

			if (nx_ctx->had_error) {
				if (plusDown)
					break;
			} else if (!nx_ctx->frame_handler.IsEmpty()) {
				HandleScope frame_scope(iso);
				TryCatch try_catch(iso);
				Local<Function> handler = nx_ctx->frame_handler.Get(iso);
				Local<Value> args[] = {Boolean::New(iso, plusDown)};
				Local<Value> ret;
				if (!handler->Call(context, Null(iso), 1, args).ToLocal(&ret)) {
					nx_emit_error_event(iso, &try_catch);
				}
				if (!is_running)
					break;
			} else if (plusDown) {
				// No frame handler registered (e.g. a script that finished its
				// work, like the test runner): + must still exit cleanly so we
				// reach teardown (incl. horizon_mman_teardown()). Without this
				// the loop ran forever and the only way out was a force-quit,
				// which skips teardown and leaks V8's svcMapMemory arenas ->
				// the NEXT launch crashes in Arena::CommitRange/_malloc_r.
				break;
			}

			if (nx_ctx->rendering_mode == NX_RENDERING_MODE_CONSOLE) {
				consoleUpdate(print_console);
			} else if (nx_ctx->rendering_mode == NX_RENDERING_MODE_CANVAS) {
				if (nx_webgl_active()) {
					// WebGL2 screen context: the app rendered directly into
					// the EGL back buffer (FBO 0); swap if it drew anything
					// since the last present.
					nx_webgl_present();
				} else if (screen_is_gpu) {
					// Composite the persistent canvas surface into the EGL back
					// buffer + swap. Presenting every frame is correct because
					// the persistent surface always holds full current content
					// (see nx_skia_gpu_present); double buffering no longer
					// causes flicker.
					nx_skia_gpu_present();
				} else if (framebuffer != NULL && js_framebuffer != NULL) {
					u32 stride;
					u8 *fb = (u8 *)framebufferBegin(framebuffer, &stride);
					// framebufferBegin() can return NULL when the NWindow is
					// in a bad state (e.g. a failed WebGL EGL bring-up touched
					// the window before falling back to raster). Skip the
					// frame instead of memcpy()ing into NULL.
					if (fb != NULL) {
						memcpy(fb, js_framebuffer,
						       js_fb_width * js_fb_height * 4);
						framebufferEnd(framebuffer);
					}
				}
			}
		}

		// ---- Exit handler ------------------------------------------------
		if (!nx_ctx->exit_handler.IsEmpty()) {
			HandleScope exit_scope(iso);
			TryCatch try_catch(iso);
			Local<Function> handler = nx_ctx->exit_handler.Get(iso);
			Local<Value> ret;
			if (handler->Call(context, Null(iso), 0, NULL).ToLocal(&ret)) {
				// exit handler result ignored
			}
		}
	}

	if (nx_ctx->rendering_mode == NX_RENDERING_MODE_CONSOLE) {
		nx_console_exit();
	} else if (nx_ctx->rendering_mode == NX_RENDERING_MODE_CANVAS) {
		if (nx_webgl_active()) {
			nx_webgl_exit();
		} else if (screen_is_gpu) {
			nx_skia_gpu_screen_exit();
		} else {
			nx_framebuffer_exit();
		}
	}

	// Release retained handles before disposing the isolate.
	nx_modules_teardown();
	nx_ctx->frame_handler.Reset();
	nx_ctx->exit_handler.Reset();
	nx_ctx->error_handler.Reset();
	nx_ctx->unhandled_rejection_handler.Reset();
	nx_ctx->unhandled_rejected_promise.Reset();
	nx_ctx->init_obj.Reset();

	// Force-close any libuv handles still on the loop (e.g. socket poll handles
	// left over from an app that was exited mid-flight). uv_poll_t does NOT own
	// its fd, so for poll handles we ALSO close the underlying BSD fd here --
	// otherwise socketExit() below would tear down the bsdsocket session with
	// live sessions still open, which faults the bsdsocket sysmodule (User
	// Break) and corrupts the next launch. Then run the loop until all close
	// callbacks fire so uv_loop_close() succeeds (it returns EBUSY otherwise).
	uv_walk(
	    &loop,
	    [](uv_handle_t *handle, void *) {
		    if (uv_is_closing(handle))
			    return;
		    if (handle->type == UV_POLL) {
			    uv_os_fd_t fd = -1;
			    if (uv_fileno(handle, &fd) == 0 && fd >= 0) {
				    close(fd);
			    }
		    }
		    uv_close(handle, nullptr);
	    },
	    nullptr);
	// Pump until no active/closing handles remain (bounded to avoid a hang).
	for (int i = 0; i < 1000 && uv_run(&loop, UV_RUN_NOWAIT) != 0; i++) {
	}
	uv_loop_close(&loop);

	iso->Dispose();
	delete create_params.array_buffer_allocator;
	V8::Dispose();
	V8::DisposePlatform();

	// Library / heap cleanup. IMPORTANT: everything that touches the normal
	// heap or a libnx service (mbedtls frees, FreeType, spl/pl/romfs/socket
	// Exit) MUST run while the address space is still intact, i.e. BEFORE
	// horizon_mman_teardown() unmaps V8's arena reservation (which is carved
	// from the stack/virtmem region). Calling a service after teardown runs it
	// in a half-dismantled address space and corrupts the sysmodule session
	// (observed: bsdsocket User Break + hbmenu null-deref on the next launch).
	if (nx_ctx->ft_library) {
		FT_Done_FreeType(nx_ctx->ft_library);
	}
	if (nx_ctx->mbedtls_initialized) {
		mbedtls_ctr_drbg_free(&nx_ctx->ctr_drbg);
		mbedtls_entropy_free(&nx_ctx->entropy);
	}
	if (nx_ctx->ca_certs_loaded) {
		mbedtls_x509_crt_free(&nx_ctx->ca_chain);
	}
	if (nx_ctx->spl_initialized) {
		splExit();
	}
	nx_hidsys_exit(nx_ctx);
	nx_config_free(&nx_ctx->config);
	free(nx_ctx);

	// Must run while the socket layer is still up: libuv's async self-wakeup
	// pipe is a loopback TCP socket pair (libnx has no anonymous pipes), and
	// uv_library_shutdown() closes those socket fds. The switch-libuv port
	// disables the destructor that would otherwise call it at libc teardown
	// (which runs after socketExit), so the embedder must call it here.
	uv_library_shutdown();

	// Close libnx services in reverse init order, while memory is still valid.
	nx_display_parachute_release();
	if (g_nv_early_ref) {
		nvExit();
		g_nv_early_ref = false;
	}
	plExit();
	romfsUnmount("nxjs");
	// The `romfs:` mount (self for a standalone app, or the launched app's NRO
	// for a bootstrap launch) is unmounted best-effort; it is a no-op if no
	// such mount was registered.
	romfsUnmount("romfs");
	socketExit();

	if (debug_fd) {
		fclose(debug_fd);
	}
	delete_if_empty(LOG_FILENAME);

	// CRITICAL: V8 maps its arenas via manual
	// svcMapMemory, which libnx's NRO exit does NOT unmap. Release them as the
	// VERY LAST thing before returning to hbloader/hbmenu — leaked aliases
	// corrupt the next process's address space (posix_memalign->NULL, hbmenu
	// framebuffer crash). Matches the hello-v8 reference teardown ordering.
	horizon_mman_teardown();
	return 0;
}
