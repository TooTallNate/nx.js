/**
 * nxjs-test — host-platform conformance test driver (V8 + libuv era).
 *
 * Mirrors source/main.cc's V8 + libuv runtime setup, but for the host:
 *   - libnx is stubbed (compat/ + stubs.cc); Switch-only modules are no-ops.
 *   - Entry is a test driver: argv[1] = runtime.js, argv[2] = fixture script.
 *   - CA certs are loaded from host paths (not the Switch SSL service).
 *   - The screen canvas is raster (no EGL/Ganesh GPU on host).
 *
 * It builds the same `$` init object, runs the embedded-equivalent runtime.js
 * (passed as a file) then the fixture as an ES module, and pumps the libuv loop
 * + V8 microtasks until the script settles, so async fixtures (fetch, crypto,
 * timers) complete. TAP output goes to stdout for the vitest comparison.
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ada.h>
#include <ft2build.h>
#include <harfbuzz/hb.h>
#include <libplatform/libplatform.h>
#include <mbedtls/version.h>
#include <png.h>
#include <turbojpeg.h>
#include <uv.h>
#include <v8.h>
#include <webp/decode.h>
#include <zlib.h>
#include <zstd.h>
#include FT_FREETYPE_H

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/psa_util.h>
#include <psa/crypto.h>

#include "error.h"
#include "module.h"
#include "types.h"
#include "util.h"

#include "include/core/SkMilestone.h"

using namespace v8;

// ---------------------------------------------------------------------------
// Module init declarations (BINDINGS.md signature).
// ---------------------------------------------------------------------------
#define NX_MOD(name)                                                           \
	void nx_init_##name(v8::Isolate *, v8::Local<v8::Object>)
NX_MOD(account); NX_MOD(album); NX_MOD(applet); NX_MOD(audio); NX_MOD(battery);
NX_MOD(bluetooth);
NX_MOD(canvas); NX_MOD(compression); NX_MOD(crypto); NX_MOD(dns);
NX_MOD(dommatrix); NX_MOD(error); NX_MOD(font); NX_MOD(fs); NX_MOD(fsdev);
NX_MOD(gamepad); NX_MOD(hidsys); NX_MOD(image); NX_MOD(irs); NX_MOD(memory);
NX_MOD(nifm);
NX_MOD(ns); NX_MOD(path2d); NX_MOD(service); NX_MOD(swkbd); NX_MOD(tcp);
NX_MOD(tls); NX_MOD(udp); NX_MOD(url); NX_MOD(web); NX_MOD(window);
#undef NX_MOD

// canvas raster present accessor (provided by canvas.cc).
struct nx_canvas_s;
extern uint8_t *nx_canvas_pixels(struct nx_canvas_s *c);
extern uint32_t nx_canvas_width(struct nx_canvas_s *c);
extern uint32_t nx_canvas_height(struct nx_canvas_s *c);
struct nx_canvas_s *nx_get_canvas(Isolate *iso, Local<Value> obj);

// ---------------------------------------------------------------------------
// Loop / exit state
// ---------------------------------------------------------------------------
static int is_running = 1;
void nx_exit_event_loop(void) { is_running = 0; }

// ---------------------------------------------------------------------------
// Host stubs for the Switch-only `$` helpers (HID, console, framebuffer).
// ---------------------------------------------------------------------------
void nx_console_init(nx_context_t *) {}
void nx_console_exit(void) {}
void nx_framebuffer_exit(void) {}

static void js_exit(const FunctionCallbackInfo<Value> &info) {
	(void)info;
	is_running = 0;
}

// queueMicrotask(callback) — mirrors source/main.cc (raw V8 has no such global).
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
	Global<Function> *gfn = new Global<Function>(iso, info[0].As<Function>());
	iso->EnqueueMicrotask(nx_microtask_run, gfn);
}
static void js_cwd(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	char cwd[4096];
	if (getcwd(cwd, sizeof(cwd) - 1)) {
		size_t n = strlen(cwd);
		if (n && cwd[n - 1] != '/') {
			cwd[n] = '/';
			cwd[n + 1] = '\0';
		}
		info.GetReturnValue().Set(nx_str(iso, cwd));
	}
}
static void js_chdir(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value dir(iso, info[0]);
	if (*dir)
		chdir(*dir);
}
static void js_print(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value s(iso, info[0]);
	if (*s)
		fputs(*s, stdout);
}
static void js_print_err(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value s(iso, info[0]);
	if (*s)
		fputs(*s, stderr);
}
static void js_getenv(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value name(iso, info[0]);
	if (!*name)
		return;
	const char *v = getenv(*name);
	if (v)
		info.GetReturnValue().Set(nx_str(iso, v));
}
static void js_setenv(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value name(iso, info[0]);
	String::Utf8Value val(iso, info[1]);
	if (*name && *val)
		setenv(*name, *val, 1);
}
static void js_unsetenv(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value name(iso, info[0]);
	if (*name)
		unsetenv(*name);
}
static void js_env_to_object(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	Local<Object> obj = Object::New(iso);
	extern char **environ;
	for (char **e = environ; *e; e++) {
		const char *eq = strchr(*e, '=');
		if (!eq)
			continue;
		Local<String> k =
		    String::NewFromUtf8(iso, *e, NewStringType::kNormal,
		                        (int)(eq - *e))
		        .ToLocalChecked();
		obj->Set(ctx, k, nx_str(iso, eq + 1)).Check();
	}
	info.GetReturnValue().Set(obj);
}

// HID stubs (host has no controllers / touch / keyboard).
static void js_hid_noop(const FunctionCallbackInfo<Value> &info) { (void)info; }
static void js_hid_get_touch_screen_states(
    const FunctionCallbackInfo<Value> &info) {
	(void)info;
}
static void js_hid_get_keyboard_states(
    const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	Local<Object> obj = Object::New(iso);
	obj->Set(ctx, nx_str(iso, "modifiers"),
	         BigInt::NewFromUnsigned(iso, 0))
	    .Check();
	for (int i = 0; i < 4; i++)
		obj->Set(ctx, i, BigInt::NewFromUnsigned(iso, 0)).Check();
	info.GetReturnValue().Set(obj);
}

// ---------------------------------------------------------------------------
// Frame / exit / promise-rejection handlers (mirror source/main.cc).
// ---------------------------------------------------------------------------
static void nx_set_frame_handler(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_ctx(iso)->frame_handler.Reset(iso, info[0].As<Function>());
}
static void nx_set_exit_handler(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_ctx(iso)->exit_handler.Reset(iso, info[0].As<Function>());
}

// Raster framebuffer init: just flips into canvas mode (no display on host).
static void nx_framebuffer_init(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_context_t *ctx = nx_ctx(iso);
	struct nx_canvas_s *canvas = nx_get_canvas(iso, info[0]);
	if (!canvas)
		return;
	ctx->rendering_mode = NX_RENDERING_MODE_CANVAS;
}

static void js_get_internal_promise_state(
    const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	if (!info[0]->IsPromise()) {
		info.GetReturnValue().Set(Null(iso));
		return;
	}
	Local<Promise> promise = info[0].As<Promise>();
	Local<Array> arr = Array::New(iso, 2);
	Promise::PromiseState state = promise->State();
	arr->Set(context, 0, Integer::New(iso, state)).Check();
	if (state == Promise::kPending)
		arr->Set(context, 1, Null(iso)).Check();
	else
		arr->Set(context, 1, promise->Result()).Check();
	info.GetReturnValue().Set(arr);
}

// version accessors (host: no ams/emummc).
static void nx_version_get_ams(Local<Name>,
                               const PropertyCallbackInfo<Value> &info) {
	info.GetReturnValue().Set(Undefined(info.GetIsolate()));
}
static void nx_version_get_emummc(Local<Name>,
                                  const PropertyCallbackInfo<Value> &info) {
	info.GetReturnValue().Set(Undefined(info.GetIsolate()));
}

// print_js_error + nx_promise_rejection_handler are provided by error.cc
// (declared in error.h).

// ---------------------------------------------------------------------------
// Script / module evaluation (mirror source/main.cc)
// ---------------------------------------------------------------------------
static bool run_script(Isolate *iso, Local<Context> context, const char *src,
                       size_t len, const char *name) {
	HandleScope scope(iso);
	TryCatch try_catch(iso);
	Local<String> source;
	if (!String::NewFromUtf8(iso, src, NewStringType::kNormal, (int)len)
	         .ToLocal(&source)) {
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

// ES module loading (static `import` + filesystem `await import()`) is shared
// with the device runtime via source/module.cc (nx_init_modules /
// nx_run_entry_module).

// ---------------------------------------------------------------------------
// $ bridge
// ---------------------------------------------------------------------------
static void build_init_object(Isolate *iso, Local<Context> context,
                              Local<Object> init_obj) {
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
	nx_init_web(iso, init_obj);
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
	NX_SET_FUNC(init_obj, "hidInitializeTouchScreen", js_hid_noop);
	NX_SET_FUNC(init_obj, "hidGetTouchScreenStates",
	            js_hid_get_touch_screen_states);
	NX_SET_FUNC(init_obj, "hidInitializeKeyboard", js_hid_noop);
	NX_SET_FUNC(init_obj, "hidGetKeyboardStates", js_hid_get_keyboard_states);
	NX_SET_FUNC(init_obj, "hidInitializeVibrationDevices", js_hid_noop);
	NX_SET_FUNC(init_obj, "hidSendVibrationValues", js_hid_noop);

	Local<Object> version = Object::New(iso);
	auto set_ver = [&](const char *k, const char *v) {
		version->Set(context, nx_str(iso, k), nx_str(iso, v)).Check();
	};
	set_ver("ada", ADA_VERSION);
	version
	    ->SetNativeDataProperty(context, nx_str(iso, "ams"), nx_version_get_ams)
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
	set_ver("hos", "0.0.0");
	set_ver("libnx", "0.0.0");
	set_ver("mbedtls", MBEDTLS_VERSION_STRING);
	set_ver("nxjs", "0.0.0-test");
	{
		char skia[8];
		snprintf(skia, sizeof(skia), "%d", SK_MILESTONE);
		set_ver("skia", skia);
	}
	set_ver("png", PNG_LIBPNG_VER_STRING);
	set_ver("turbojpeg", "0");
	set_ver("v8", V8::GetVersion());
	{
		char z[16];
		snprintf(z, sizeof(z), "%u", (unsigned)ZSTD_versionNumber());
		set_ver("zstd", z);
	}
	set_ver("zlib", ZLIB_VERSION);
	init_obj->Set(context, nx_str(iso, "version"), version).Check();

	// Host harness has no applet regime; report a 1 MiB rx buffer (matches the
	// device application config) so the JS socket layer sizes its read buffer.
	init_obj
	    ->Set(context, nx_str(iso, "tcpRxBufSize"),
	          Integer::NewFromUnsigned(iso, 1024u * 1024u))
	    .Check();

	// `$.config`: the host reads no nxjs.ini, so expose defaults matching the
	// device application regime. Mirrors the device build_init_object so
	// fixtures that read `$.config` see the same shape.
	{
		Local<Object> conf = Object::New(iso);
		auto cset = [&](const char *k, Local<Value> v) {
			conf->Set(context, nx_str(iso, k), v).Check();
		};
		cset("jit", Boolean::New(iso, true));
		// Match the device application-mode default (64 MiB WASM headroom) so
		// the runtime's WebAssembly-availability guard (see index.ts) keeps
		// WASM enabled on the host harness — the wasm.ts fixture relies on it.
		cset("codeHeadroomMb", Integer::NewFromUnsigned(iso, 64));
		// Device exposes the effective (post-clamp) max heap in bytes, never 0;
		// report the device application-regime cap (512 MiB) so host fixtures
		// see the same semantics.
		cset("heapLimit", Number::New(iso, 512.0 * 1024 * 1024));
		cset("renderer", nx_str(iso, "auto"));
		cset("v8Flags", nx_str(iso, ""));

		Local<Object> sock = Object::New(iso);
		auto sset = [&](const char *k, uint32_t v) {
			sock->Set(context, nx_str(iso, k),
			          Integer::NewFromUnsigned(iso, v))
			    .Check();
		};
		sset("tcpTxBufSize", 256u * 1024u);
		sset("tcpRxBufSize", 256u * 1024u);
		sset("tcpTxBufMaxSize", 1024u * 1024u);
		sset("tcpRxBufMaxSize", 1024u * 1024u);
		sset("udpTxBufSize", 0x2400u);
		sset("udpRxBufSize", 0xA500u);
		sset("sbEfficiency", 6u);
		sset("numBsdSessions", 3u);
		sset("serviceType", 3u); // BsdServiceType_Auto
		conf->Set(context, nx_str(iso, "socket"), sock).Check();

		// `$.config.console`: the host reads no nxjs.ini, so expose an empty
		// object (no overrides) to match the device `$.config` shape.
		conf->Set(context, nx_str(iso, "console"), Object::New(iso)).Check();

		conf->Set(context, nx_str(iso, "loaded"), Boolean::New(iso, false))
		    .Check();
		init_obj->Set(context, nx_str(iso, "config"), conf).Check();
	}

	// `$.argv`: the device exposes a string[] (process argv). The host harness
	// has no app argv, but expose an empty array so the `$` shape matches the
	// device (`argv: string[]`) and `$.argv[0]` is a well-defined `undefined`
	// rather than throwing on a missing property (e.g. via `Application.self`).
	init_obj->Set(context, nx_str(iso, "argv"), Array::New(iso, 0)).Check();

	// `$.selfNroPath`: the device sets the launched app's NRO path (or null for
	// installed titles). The host has no app; expose null to match the `$` shape
	// (`selfNroPath: string | null`). `Application.self` is `ns`-stubbed here.
	init_obj->Set(context, nx_str(iso, "selfNroPath"), Null(iso)).Check();
}

// ---------------------------------------------------------------------------
// File reader
// ---------------------------------------------------------------------------
static char *read_file(const char *path, size_t *out_len) {
	FILE *f = fopen(path, "rb");
	if (!f)
		return nullptr;
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *buf = (char *)malloc(n + 1);
	if (!buf) {
		fclose(f);
		return nullptr;
	}
	size_t rd = fread(buf, 1, n, f);
	fclose(f);
	buf[rd] = '\0';
	if (out_len)
		*out_len = rd;
	return buf;
}

// Decode a base64 data: URL payload (no whitespace) into a malloc'd buffer.
static uint8_t *base64_decode(const char *b64, size_t b64_len, size_t *out_len) {
	static const int8_t T[256] = {
	    /* fully initialized below */
	};
	(void)T;
	int8_t tbl[256];
	for (int i = 0; i < 256; i++)
		tbl[i] = -1;
	const char *alpha =
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	for (int i = 0; i < 64; i++)
		tbl[(unsigned char)alpha[i]] = (int8_t)i;
	uint8_t *out = (uint8_t *)malloc(b64_len / 4 * 3 + 4);
	if (!out)
		return nullptr;
	size_t o = 0;
	int val = 0, bits = 0;
	for (size_t i = 0; i < b64_len; i++) {
		char c = b64[i];
		if (c == '=' || c == '\n' || c == '\r')
			continue;
		int8_t d = tbl[(unsigned char)c];
		if (d < 0)
			continue;
		val = (val << 6) | d;
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			out[o++] = (uint8_t)((val >> bits) & 0xFF);
		}
	}
	*out_len = o;
	return out;
}

// PNG render mode: after the fixture has drawn into the canvas stashed at
// globalThis.__nxjs_canvas__, encode it to PNG via the runtime's synchronous
// toDataURL() (Skia) and write the bytes to `out_path`. Returns 0 on success.
static int write_canvas_png(Isolate *iso, Local<Context> context,
                            const char *out_path) {
	HandleScope scope(iso);
	TryCatch try_catch(iso);
	Local<Object> global = context->Global();
	Local<Value> canvas_v;
	if (!global->Get(context, nx_str(iso, "__nxjs_canvas__")).ToLocal(&canvas_v) ||
	    !canvas_v->IsObject()) {
		fprintf(stderr,
		        "no canvas found — did the fixture call createCanvas()?\n");
		return 1;
	}
	Local<Object> canvas = canvas_v.As<Object>();
	Local<Value> to_data_url_v;
	if (!canvas->Get(context, nx_str(iso, "toDataURL")).ToLocal(&to_data_url_v) ||
	    !to_data_url_v->IsFunction()) {
		fprintf(stderr, "canvas.toDataURL is not a function\n");
		return 1;
	}
	Local<Value> arg = nx_str(iso, "image/png");
	Local<Value> result;
	if (!to_data_url_v.As<Function>()
	         ->Call(context, canvas, 1, &arg)
	         .ToLocal(&result)) {
		print_js_error(iso, &try_catch);
		return 1;
	}
	String::Utf8Value data_url(iso, result);
	if (!*data_url) {
		fprintf(stderr, "toDataURL returned empty\n");
		return 1;
	}
	const char *comma = strchr(*data_url, ',');
	if (!comma) {
		fprintf(stderr, "malformed data URL\n");
		return 1;
	}
	const char *b64 = comma + 1;
	size_t b64_len = data_url.length() - (b64 - *data_url);
	size_t png_len = 0;
	uint8_t *png = base64_decode(b64, b64_len, &png_len);
	if (!png) {
		fprintf(stderr, "base64 decode failed\n");
		return 1;
	}
	FILE *f = fopen(out_path, "wb");
	if (!f) {
		fprintf(stderr, "cannot open output: %s\n", out_path);
		free(png);
		return 1;
	}
	fwrite(png, 1, png_len, f);
	fclose(f);
	free(png);
	return 0;
}

static void load_host_ca_certs(nx_context_t *nx) {
	mbedtls_x509_crt_init(&nx->ca_chain);
	const char *paths[] = {"/etc/ssl/cert.pem",
	                       "/etc/ssl/certs/ca-certificates.crt",
	                       "/etc/pki/tls/certs/ca-bundle.crt",
	                       "/opt/homebrew/etc/openssl@3/cert.pem", nullptr};
	for (int i = 0; paths[i]; i++) {
		if (mbedtls_x509_crt_parse_file(&nx->ca_chain, paths[i]) >= 0) {
			nx->ca_certs_loaded = true;
			return;
		}
	}
	fprintf(stderr, "warning: no host CA certs found; TLS verify disabled\n");
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "usage: %s <runtime.js> <fixture.js> [args...]\n",
		        argv[0]);
		return 1;
	}
	const char *runtime_path = argv[1];
	const char *script_path = argv[2];

	// Optional PNG render mode: "--png <output.png> <width> <height>".
	// Renders the fixture's canvas to a PNG instead of running the TAP loop,
	// used by the canvas image-conformance tests (test/canvas).
	const char *png_out = nullptr;
	int png_w = 200, png_h = 200;
	for (int i = 3; i < argc; i++) {
		if (strcmp(argv[i], "--png") == 0 && i + 3 < argc) {
			png_out = argv[i + 1];
			png_w = atoi(argv[i + 2]);
			png_h = atoi(argv[i + 3]);
			i += 3;
		}
	}

	if (psa_crypto_init() != PSA_SUCCESS) {
		fprintf(stderr, "psa_crypto_init failed\n");
		return 1;
	}

	nx_context_t *nx_ctx = (nx_context_t *)calloc(1, sizeof(nx_context_t));
	nx_ctx->rendering_mode = NX_RENDERING_MODE_INIT;
	load_host_ca_certs(nx_ctx);

	uv_loop_t loop;
	uv_loop_init(&loop);
	nx_ctx->loop = &loop;

	std::unique_ptr<Platform> platform =
	    platform::NewSingleThreadedDefaultPlatform();
	V8::InitializePlatform(platform.get());
	V8::SetFlagsFromString("--single-threaded --single-threaded-gc");
	V8::Initialize();

	Isolate::CreateParams create_params;
	create_params.array_buffer_allocator =
	    ArrayBuffer::Allocator::NewDefaultAllocator();
	Isolate *iso = Isolate::New(create_params);
	nx_ctx->iso = iso;
	iso->SetData(0, nx_ctx);
	iso->SetPromiseRejectCallback(nx_promise_rejection_handler);
	nx_init_modules(iso); // import.meta + static/dynamic import (module.cc)
	iso->SetMicrotasksPolicy(MicrotasksPolicy::kExplicit);

	int exit_code = 0;
	{
		Isolate::Scope iso_scope(iso);
		HandleScope handle_scope(iso);
		Local<Context> context = Context::New(iso);
		Context::Scope context_scope(context);
		Local<Object> global = context->Global();

		Local<Object> init_obj = Object::New(iso);
		nx_ctx->init_obj.Reset(iso, init_obj);
		build_init_object(iso, context, init_obj);
		global->Set(context, nx_str(iso, "$"), init_obj).Check();

		size_t rt_len = 0, sc_len = 0;
		char *rt_src = read_file(runtime_path, &rt_len);
		char *sc_src = read_file(script_path, &sc_len);
		if (!rt_src || !sc_src) {
			fprintf(stderr, "failed to read runtime/script\n");
			exit_code = 1;
		} else if (!run_script(iso, context, rt_src, rt_len, "runtime.js")) {
			exit_code = 1;
		} else if (png_out) {
			// Canvas image mode: expose createCanvas() (backed by the runtime's
			// OffscreenCanvas), run the fixture as a classic script, then encode
			// the resulting canvas to PNG. The fixtures are shared verbatim with
			// Chrome, which gets createCanvas() from the test harness page.
			char cc[512];
			snprintf(cc, sizeof(cc),
			         "globalThis.createCanvas = function(w, h) {\n"
			         "  if (w === undefined) w = %d;\n"
			         "  if (h === undefined) h = %d;\n"
			         "  var canvas = new OffscreenCanvas(w, h);\n"
			         "  var ctx = canvas.getContext('2d');\n"
			         "  globalThis.__nxjs_canvas__ = canvas;\n"
			         "  return { canvas: canvas, ctx: ctx };\n"
			         "};\n",
			         png_w, png_h);
			if (!run_script(iso, context, cc, strlen(cc), "<createCanvas>")) {
				exit_code = 1;
			} else if (!run_script(iso, context, sc_src, sc_len,
			                       script_path)) {
				exit_code = 1;
			} else {
				iso->PerformMicrotaskCheckpoint();
				exit_code = write_canvas_png(iso, context, png_out);
			}
		} else {
			char url[4096];
			snprintf(url, sizeof(url), "file://%s", script_path);
			nx_run_entry_module(iso, context, sc_src, sc_len, url);
		}
		free(rt_src);
		free(sc_src);

		// Drive the runtime until the fixture settles or Switch.exit() is
		// called (tap.ts calls Switch.exit() once all tests complete, which
		// flips `is_running` to false).
		//
		// nx.js implements setTimeout/setInterval purely in JS (see
		// src/timers.ts): timers fire from processTimers(), which the runtime
		// drives from its per-frame handler ($.onFrame in src/index.ts). So
		// runtime.js *always* registers a frame handler. To make wall-clock
		// timers (e.g. a 200ms setTimeout) actually elapse, we must pace the
		// frame loop like a real ~60fps display rather than busy-spinning:
		// each iteration we pump libuv + microtasks, invoke the frame handler,
		// then sleep ~16ms so Date.now() advances realistically.
		//
		// The cap is a wall-clock safety budget (~60s) so a hung fixture can't
		// block CI forever. UV_RUN_NOWAIT (not ONCE) because our own sleep
		// provides the frame pacing and we must not block past one frame.
		const int kFrameMs = 16;
		const int kMaxFrames = 60000 / kFrameMs * 60; // ~60s budget
		int idle = 0;
		// PNG render mode is synchronous and already produced its output, so
		// skip the TAP frame loop entirely.
		for (int i = 0; !png_out && is_running && i < kMaxFrames; i++) {
			bool has_frame = !nx_ctx->frame_handler.IsEmpty();
			int alive = uv_run(&loop, UV_RUN_NOWAIT);
			iso->PerformMicrotaskCheckpoint();
			if (has_frame) {
				HandleScope fs(iso);
				Local<Function> fn = nx_ctx->frame_handler.Get(iso);
				Local<Value> a[] = {Boolean::New(iso, false)};
				(void)fn->Call(context, Null(iso), 1, a);
				iso->PerformMicrotaskCheckpoint();
			}
			if (!alive && !has_frame) {
				if (++idle > 3)
					break;  // nothing left to do
			} else {
				idle = 0;
			}
			// Pace the loop so JS timers (driven off Date.now()) elapse in
			// realistic wall-clock time. Skip the sleep once nothing is left.
			if (is_running)
				uv_sleep(kFrameMs);
		}

		if (!nx_ctx->exit_handler.IsEmpty()) {
			HandleScope es(iso);
			Local<Function> fn = nx_ctx->exit_handler.Get(iso);
			(void)fn->Call(context, Null(iso), 0, nullptr);
		}
		nx_ctx->frame_handler.Reset();
		nx_ctx->exit_handler.Reset();
		nx_ctx->init_obj.Reset();
		nx_ctx->unhandled_rejected_promise.Reset();
		// Release the module cache's Global<Module> handles while the isolate
		// is still alive. Without this the static map in module.cc is destroyed
		// at process exit, AFTER iso->Dispose() / V8::Dispose(), and the
		// Global<> destructors touch a disposed isolate -> segfault. (The device
		// runtime calls this before its iso->Dispose() too.)
		nx_modules_teardown();
	}

	uv_walk(
	    &loop,
	    [](uv_handle_t *h, void *) {
		    if (!uv_is_closing(h))
			    uv_close(h, nullptr);
	    },
	    nullptr);
	for (int i = 0; i < 1000 && uv_run(&loop, UV_RUN_NOWAIT) != 0; i++) {
	}
	uv_loop_close(&loop);

	iso->Dispose();
	delete create_params.array_buffer_allocator;
	V8::Dispose();
	V8::DisposePlatform();
	uv_library_shutdown();
	free(nx_ctx);
	return exit_code;
}
