#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
#include "skia_gpu.h"
#include "types.h"
#include "util.h"

#include "include/core/SkMilestone.h"
#include "include/core/SkSurface.h"

// Phase 2.2: the screen canvas is backed by a GPU SkSurface (EGL FBO 0) and
// presented via eglSwapBuffers when GPU bringup succeeds. If it fails (e.g.
// Mesa starved for memory in applet mode), we fall back to a raster SkSurface
// presented by blitting into the libnx framebuffer (the proven Phase 2.1
// path). `screen_is_gpu` records which path is active for the present loop.
static bool screen_is_gpu = false;

// Module init declarations (each defined in its own .cc). Signature per
// BINDINGS.md: void nx_init_foo(v8::Isolate*, v8::Local<v8::Object> init_obj).
#define NX_MODULE(name)                                                        \
	void nx_init_##name(v8::Isolate *, v8::Local<v8::Object>)
NX_MODULE(account);
NX_MODULE(album);
NX_MODULE(applet);
NX_MODULE(audio);
NX_MODULE(battery);
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
NX_MODULE(service);
NX_MODULE(swkbd);
NX_MODULE(tcp);
NX_MODULE(tls);
NX_MODULE(udp);
NX_MODULE(url);
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

// ---------------------------------------------------------------------------
// Core `$` functions implemented here in main.cc.
// ---------------------------------------------------------------------------

static void js_exit(const FunctionCallbackInfo<Value> &info) {
	is_running = 0;
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
		char *eq = strchr(key, '=');
		if (eq) {
			*eq = '\0';
			env->Set(context, nx_str(iso, key), nx_str(iso, eq + 1)).Check();
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

	// Phase 2.2: try to back the screen canvas with a GPU SkSurface (EGL FBO 0)
	// presented via eglSwapBuffers. On any failure (e.g. Mesa OOM in applet
	// mode), fall back to the proven raster + libnx-framebuffer path.
	sk_sp<SkSurface> gpu = nx_skia_gpu_screen_init(width, height);
	if (gpu) {
		nx_canvas_set_gpu_surface(canvas, gpu);
		screen_is_gpu = true;
		js_framebuffer = nullptr;
	} else {
		fprintf(stderr, "[skia] GPU screen unavailable; using raster path\n");
		fflush(stderr);
		screen_is_gpu = false;
		js_framebuffer = nx_canvas_pixels(canvas);
		js_fb_width = width;
		js_fb_height = height;
		framebuffer = (Framebuffer *)malloc(sizeof(Framebuffer));
		framebufferCreate(framebuffer, win, width, height,
		                  PIXEL_FORMAT_BGRA_8888, 2);
		framebufferMakeLinear(framebuffer);
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
	Local<String> source =
	    String::NewFromUtf8(iso, src, NewStringType::kNormal, (int)len)
	        .ToLocalChecked();
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

// Minimal ES-module loader for the user entrypoint. (Full import resolution is
// a follow-up; for now we run the entrypoint as a module with no static
// imports resolvable, matching the common single-file app case.)
static MaybeLocal<Module> resolve_module_callback(
    Local<Context> context, Local<String> specifier,
    Local<FixedArray> import_assertions, Local<Module> referrer) {
	// TODO(phase1): real module resolution against romfs:/ + sdmc:/.
	(void)context;
	Isolate *iso = Isolate::GetCurrent();
	iso->ThrowException(
	    Exception::Error(nx_str(iso, "module resolution not implemented yet")));
	return MaybeLocal<Module>();
}

// The entrypoint module's URL, used to populate import.meta.url. Single-file
// app for now, so one global suffices.
static const char *g_entrypoint_url = "";

// Populates import.meta for the (single) entrypoint module: `url` (the module's
// resource name) and `main` (true — the entrypoint is always the main module).
static void init_import_meta(Local<Context> context, Local<Module> module,
                             Local<Object> meta) {
	Isolate *iso = Isolate::GetCurrent();
	(void)module;
	meta->CreateDataProperty(context, nx_str(iso, "url"),
	                         nx_str(iso, g_entrypoint_url))
	    .Check();
	meta->CreateDataProperty(context, nx_str(iso, "main"), True(iso)).Check();
}

static bool run_module(Isolate *iso, Local<Context> context, const char *src,
                       size_t len, const char *name) {
	HandleScope scope(iso);
	TryCatch try_catch(iso);
	g_entrypoint_url = name;
	Local<String> source =
	    String::NewFromUtf8(iso, src, NewStringType::kNormal, (int)len)
	        .ToLocalChecked();
	ScriptOrigin origin(nx_str(iso, name), 0, 0, false, -1, Local<Value>(),
	                    false, false, true /* is_module */);
	ScriptCompiler::Source script_source(source, origin);
	Local<Module> module;
	if (!ScriptCompiler::CompileModule(iso, &script_source).ToLocal(&module)) {
		nx_emit_error_event(iso, &try_catch);
		return false;
	}
	if (module->InstantiateModule(context, resolve_module_callback)
	        .IsNothing()) {
		nx_emit_error_event(iso, &try_catch);
		return false;
	}
	Local<Value> result;
	if (!module->Evaluate(context).ToLocal(&result)) {
		nx_emit_error_event(iso, &try_catch);
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// `$` bridge construction.
// ---------------------------------------------------------------------------
static void build_init_object(Isolate *iso, Local<Context> context,
                              Local<Object> init_obj, const char *entrypoint,
                              int argc, char *argv[]) {
	nx_init_account(iso, init_obj);
	nx_init_album(iso, init_obj);
	nx_init_applet(iso, init_obj);
	nx_init_audio(iso, init_obj);
	nx_init_battery(iso, init_obj);
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
	nx_init_image(iso, init_obj);
	nx_init_irs(iso, init_obj);
	nx_init_memory(iso, init_obj);
	nx_init_nifm(iso, init_obj);
	nx_init_ns(iso, init_obj);
	nx_init_service(iso, init_obj);
	nx_init_swkbd(iso, init_obj);
	nx_init_tcp(iso, init_obj);
	nx_init_tls(iso, init_obj);
	nx_init_udp(iso, init_obj);
	nx_init_url(iso, init_obj);
	nx_init_web(iso, init_obj);
	nx_init_window(iso, init_obj);

	NX_SET_FUNC(init_obj, "exit", js_exit);
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
		argv_array->Set(context, i, nx_str(iso, argv[i])).Check();
	}
	init_obj->Set(context, nx_str(iso, "argv"), argv_array).Check();
}

// BsdServiceType_Auto (tries bsd:s first) is intentional: bsd:s is required to
// bind privileged ports (e.g. a TCP server on port 80).
//
// The socket layer reserves a transfer-memory region sized roughly as
// (tcp_tx_max + tcp_rx_max + udp_tx + udp_rx) * sb_efficiency. The full config
// below is ~(4+4)MiB * 8 = ~64 MiB. That is fine in application/full-memory
// mode (GiBs free) but is catastrophic in applet mode (~137 MiB total): the
// reservation starves Mesa/V8 and bsdsocket faults the first time libuv touches
// its loopback self-wake socketpair (User Break at bsdsocket+0xe7064, observed
// at frame 1). So pick a lean config when free memory is tight.
static SocketInitConfig const s_socketInitConfigFull = {
    .tcp_tx_buf_size = 1 * 1024 * 1024,
    .tcp_rx_buf_size = 1 * 1024 * 1024,
    .tcp_tx_buf_max_size = 4 * 1024 * 1024,
    .tcp_rx_buf_max_size = 4 * 1024 * 1024,
    .udp_tx_buf_size = 0x2400,
    .udp_rx_buf_size = 0xA500,
    .sb_efficiency = 8,
    .num_bsd_sessions = 3,
    .bsd_service_type = BsdServiceType_Auto,
};

// Lean config for tight-memory (applet) mode: ~(256+256)KiB * 4 = ~2 MiB of
// transfer memory. Enough for libuv's self-wake socketpair, DNS, and modest
// fetch/TLS, while leaving room for the GPU/Mesa + V8 stack.
static SocketInitConfig const s_socketInitConfigLean = {
    .tcp_tx_buf_size = 128 * 1024,
    .tcp_rx_buf_size = 128 * 1024,
    .tcp_tx_buf_max_size = 256 * 1024,
    .tcp_rx_buf_max_size = 256 * 1024,
    .udp_tx_buf_size = 0x2400,
    .udp_rx_buf_size = 0xA500,
    .sb_efficiency = 4,
    .num_bsd_sessions = 3,
    .bsd_service_type = BsdServiceType_Auto,
};

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char *argv[]) {
	Result rc;

	nx_context_t *nx_ctx = (nx_context_t *)calloc(1, sizeof(nx_context_t));
	nx_ctx->rendering_mode = NX_RENDERING_MODE_INIT;

	rc = romfsInit();
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

	rc = socketInitialize(tight_memory ? &s_socketInitConfigLean
	                                    : &s_socketInitConfigFull);
	if (R_FAILED(rc))
		diagAbortWithResult(rc);

	rc = plInitialize(PlServiceType_User);
	if (R_FAILED(rc))
		diagAbortWithResult(rc);

	FILE *debug_fd = freopen(LOG_FILENAME, "w", stderr);

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

	// Memory-gated JIT policy (see MIGRATION-NOTES "trifecta" findings):
	// full-JIT's libnx jitCreate dual-maps a >=64 MiB code range (~128 MiB
	// real). In applet mode (~137 MiB free) that starves the GPU/Mesa stack, so
	// when free memory is tight we fall back to jitless (interpreter). Phase 1
	// uses a CPU (Cairo) canvas with no Mesa contention, so this will normally
	// pick full JIT in both regimes; the gate matters once a GPU canvas lands.
	// mem_free / tight_memory were probed before socketInitialize above.
	// ~300 MiB headroom: code arena (128 MiB real) + GPU/Mesa + JS heap.
	bool can_jit = !tight_memory;

	if (can_jit) {
		V8::SetFlagsFromString("--single-threaded --single-threaded-gc "
		                       "--predictable");
	} else {
		V8::SetFlagsFromString("--jitless --single-threaded "
		                       "--single-threaded-gc "
		                       "--no-concurrent-recompilation --predictable");
	}
	// Report the memory gate + JIT mode. When JIT is on, the linked monolith
	// tiers up Ignition -> Sparkplug -> Maglev -> TurboFan; jitless is Ignition
	// only.
	fprintf(stderr,
	        "[v8] mem_total=%llu MiB free=%llu MiB regime=%s -> mode=%s (%s)\n",
	        (unsigned long long)(mem_total / (1024 * 1024)),
	        (unsigned long long)(mem_free / (1024 * 1024)),
	        tight_memory ? "applet" : "application",
	        can_jit ? "jit" : "jitless",
	        can_jit ? "Ignition+Sparkplug+Maglev+TurboFan" : "Ignition only");
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
	// so (re)apply it AFTER. Budget the max heap from memory free at startup,
	// reserving headroom for the JIT code arena, GPU/Cairo, and native allocs.
	{
		u64 reserve = (can_jit ? 180ull : 48ull) * 1024 * 1024;
		u64 max_heap = mem_free > reserve ? mem_free - reserve : 0;
		if (max_heap < 32ull * 1024 * 1024)
			max_heap = 32ull * 1024 * 1024;
		if (max_heap > 192ull * 1024 * 1024)
			max_heap = 192ull * 1024 * 1024;
		create_params.constraints.ConfigureDefaultsFromHeapSize(
		    8ull * 1024 * 1024 /* initial */, max_heap /* max */);
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
	iso->SetHostInitializeImportMetaObjectCallback(init_import_meta);
	// Microtasks are pumped explicitly from the loop.
	iso->SetMicrotasksPolicy(MicrotasksPolicy::kExplicit);

	padConfigureInput(8, HidNpadStyleSet_NpadStandard | HidNpadStyleTag_NpadGc);
	padInitializeDefault(&nx_ctx->pads[0]);
	for (int i = 1; i < 8; i++) {
		padInitialize(&nx_ctx->pads[i],
		              (HidNpadIdType)(HidNpadIdType_No1 + i));
	}

	// Resolve the user entrypoint (romfs:/main.js, then sdmc <nro>.js).
	size_t user_code_size = 0;
	bool user_path_needs_free = false;
	char *user_code_path = (char *)"romfs:/main.js";
	char *user_code = (char *)read_file(user_code_path, &user_code_size);
	if (user_code == NULL && errno == ENOENT && argc > 0) {
		user_path_needs_free = true;
		user_code_path = strdup(argv[0]);
		if (user_code_path) {
			replace_file_extension(user_code_path, ".js");
			user_code = (char *)read_file(user_code_path, &user_code_size);
		}
	}

	{
		Isolate::Scope iso_scope(iso);
		HandleScope handle_scope(iso);
		Local<Context> context = Context::New(iso);
		Context::Scope context_scope(context);
		Local<Object> global = context->Global();

		if (user_code == NULL) {
			nx_console_init(nx_ctx);
			printf("%s: %s\n", strerror(errno), user_code_path);
			nx_ctx->had_error = 1;
		} else {
			Local<Object> init_obj = Object::New(iso);
			nx_ctx->init_obj.Reset(iso, init_obj);
			build_init_object(iso, context, init_obj, user_code_path, argc,
			                  argv);
			global->Set(context, nx_str(iso, "$"), init_obj).Check();

			// Initialize the runtime (embedded runtime.js source).
			if (!run_script(iso, context,
			                (const char *)nxjs_runtime_js,
			                nxjs_runtime_js_len, "runtime.js")) {
				nx_console_init(nx_ctx);
				printf("Runtime initialization failed\n");
				nx_ctx->had_error = 1;
			} else {
				// Run the user entrypoint as an ES module.
				run_module(iso, context, user_code, user_code_size,
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
			if (!screen_is_gpu && !applet_active)
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

			// GPU mode does not gate on appletMainLoop(), so guarantee a clean
			// exit path: + always breaks (reaching teardown). On the raster
			// path the normal frame-handler/no-handler logic below handles +.
			if (screen_is_gpu && plusDown)
				break;

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
				if (screen_is_gpu) {
					// The screen canvas drew straight into the GPU FBO; flush +
					// submit + eglSwapBuffers presents it.
					nx_skia_gpu_present();
				} else {
					u32 stride;
					u8 *fb = (u8 *)framebufferBegin(framebuffer, &stride);
					memcpy(fb, js_framebuffer,
					       js_fb_width * js_fb_height * 4);
					framebufferEnd(framebuffer);
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
		if (screen_is_gpu) {
			nx_skia_gpu_screen_exit();
		} else {
			nx_framebuffer_exit();
		}
	}

	// Release retained handles before disposing the isolate.
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
	free(nx_ctx);

	// Must run while the socket layer is still up: libuv's async self-wakeup
	// pipe is a loopback TCP socket pair (libnx has no anonymous pipes), and
	// uv_library_shutdown() closes those socket fds. The switch-libuv port
	// disables the destructor that would otherwise call it at libc teardown
	// (which runs after socketExit), so the embedder must call it here.
	uv_library_shutdown();

	// Close libnx services in reverse init order, while memory is still valid.
	plExit();
	romfsExit();
	socketExit();

	if (debug_fd) {
		fclose(debug_fd);
	}
	delete_if_empty(LOG_FILENAME);

	// CRITICAL (see MIGRATION-NOTES): V8 maps its arenas via manual
	// svcMapMemory, which libnx's NRO exit does NOT unmap. Release them as the
	// VERY LAST thing before returning to hbloader/hbmenu — leaked aliases
	// corrupt the next process's address space (posix_memalign->NULL, hbmenu
	// framebuffer crash). Matches the hello-v8 reference teardown ordering.
	horizon_mman_teardown();
	return 0;
}
