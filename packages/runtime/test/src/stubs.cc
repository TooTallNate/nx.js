/**
 * Host stubs for nxjs-test (V8 + libuv era).
 *
 * The Switch-only modules below have no host implementation, but the bundled
 * runtime.js still calls their `$` bridge functions during global setup (e.g.
 * `$.batteryInitClass(BatteryManager)`). So each stub registers the same
 * function *names* the real `source/<mod>.cc` registers, as no-ops, so the
 * runtime boots without throwing. The no-ops return `undefined`; `*New`
 * factory functions return an empty object so callers that read properties
 * off the result do not crash.
 *
 * The portable modules (audio, canvas, crypto, dns, compression, url, web,
 * fs, image, font, tcp, tls, udp, wasm, window, dommatrix, error, util,
 * async) are compiled from source/*.cc against the libnx stubs in compat/.
 * memory.cc is Switch-specific (svc memory introspection), so it is stubbed
 * here too. (audio.cc is portable because the platform output lives behind
 * audio-sink.h — the host sink is src/audio-sink.cc.)
 */
#include "types.h"
#include <v8.h>

// libnx exposes the newlib heap bounds as `fake_heap_start`/`fake_heap_end`;
// async.cc (compiled from source/) uses them to gauge native-heap pressure on
// device. The host has a normal glibc heap with no such bounds, so provide the
// symbols as a zero-width sentinel: async.cc's pressure check treats a zero
// total as "unknown" and returns early (a no-op), which is the right behavior
// on a host with plenty of RAM.
extern "C" char *fake_heap_start = nullptr;
extern "C" char *fake_heap_end = nullptr;

using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Value;

// Factory no-op: returns a fresh empty object so property reads do not crash.
static void nx_stub_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	info.GetReturnValue().Set(Object::New(iso));
}

// Boolean no-op: returns `false`. Used for predicate-shaped bindings whose
// callers branch on the result (e.g. gamepadConnectionChanged).
static void nx_stub_false(const FunctionCallbackInfo<Value> &info) {
	info.GetReturnValue().Set(v8::Boolean::New(info.GetIsolate(), false));
}

// Undefined no-op: for factory-shaped bindings whose callers must treat a
// missing host implementation as "unavailable" (e.g. webglContextNew — the
// TS getContext('webgl2') then returns null).
static void nx_stub_undefined(const FunctionCallbackInfo<Value> &info) {
	(void)info;
}

// Numeric-zero no-op. Used for appletGetAppletType so the host reports
// AppletType.Application (0) — matching the rest of the harness, which mirrors
// the device application regime (e.g. fs.ts picks the 1 MiB stream chunk size).
static void nx_stub_zero(const FunctionCallbackInfo<Value> &info) {
	info.GetReturnValue().Set(v8::Integer::New(info.GetIsolate(), 0));
}

// Register a list of names on init_obj, all bound to `fn`.
static void nx_stub_register(Isolate *iso, Local<Object> init_obj,
                             const char *const *names, size_t count,
                             v8::FunctionCallback fn) {
	Local<v8::Context> ctx = iso->GetCurrentContext();
	for (size_t i = 0; i < count; i++) {
		init_obj
		    ->Set(ctx, nx_str(iso, names[i]),
		          v8::FunctionTemplate::New(iso, fn)
		              ->GetFunction(ctx)
		              .ToLocalChecked())
		    .Check();
	}
}

#define NX_STUB_NAMES(...)                                                     \
	static const char *const names[] = {__VA_ARGS__};                          \
	nx_stub_register(iso, init_obj, names, countof(names), nx_stub_new);

#define NX_STUB_FN(name)                                                       \
	void nx_init_##name(Isolate *iso, Local<Object> init_obj)

NX_STUB_FN(account) {
	NX_STUB_NAMES("accountInitialize", "accountProfileInit",
	              "accountCurrentProfile", "accountSelectProfile",
	              "accountProfileNew", "accountProfiles")
}

NX_STUB_FN(album) {
	NX_STUB_NAMES("capsaInitialize", "albumInit", "albumFileInit",
	              "albumFileList")
}

NX_STUB_FN(applet) {
	NX_STUB_NAMES("appletIlluminance", "appletGetOperationMode",
	              "appletSetMediaPlaybackState")
	// appletGetAppletType: return 0 (AppletType.Application) so fs.ts and any
	// other regime-gated code takes the application-mode path on the host.
	static const char *const zero_names[] = {"appletGetAppletType"};
	nx_stub_register(iso, init_obj, zero_names, countof(zero_names),
	                 nx_stub_zero);
}

NX_STUB_FN(battery) {
	NX_STUB_NAMES("batteryInit", "batteryInitClass", "batteryExit")
}

NX_STUB_FN(bluetooth) {
	NX_STUB_NAMES("btleInit", "btleExit", "btleScanStart", "btleScanStop",
	              "btleScanResults", "btleConnect", "btleDisconnect",
	              "btleConnections", "btleGetServices",
	              "btleGetCharacteristics", "btleGetDescriptors", "btleRead",
	              "btleWrite", "btleWriteDescriptor", "btleNotify",
	              "btlePollEvents")
}

NX_STUB_FN(fsdev) {
	NX_STUB_NAMES("fsInit", "fsMount", "fsOpenBis", "fsOpenSdmc",
	              "fsOpenWithId", "saveDataInit", "saveDataMount",
	              "saveDataCreateSync", "fsOpenSaveDataInfoReader",
	              "fsSaveDataInfoReaderNext")
}

NX_STUB_FN(gamepad) {
	NX_STUB_NAMES("gamepadInit", "gamepadNew", "gamepadButtonInit",
	              "gamepadButtonNew")
}

// `hid:sys` (gamepad serial id resolution + connect/disconnect event) is
// libnx-only, so it is stubbed host-side. gamepadConnectionChanged must return
// a boolean (false) — the JS connection sweep branches on it.
NX_STUB_FN(hidsys) {
	static const char *const names[] = {"gamepadConnectionChanged"};
	nx_stub_register(iso, init_obj, names, countof(names), nx_stub_false);
}

NX_STUB_FN(irs) {
	NX_STUB_NAMES("irsInit", "irsSensorNew", "irsSensorStart", "irsSensorStop",
	              "irsSensorUpdate")
}

NX_STUB_FN(memory) {
	NX_STUB_NAMES("memoryUsage")
}

NX_STUB_FN(nifm) {
	NX_STUB_NAMES("nifmInitialize", "networkInfo")
}

NX_STUB_FN(ns) {
	NX_STUB_NAMES("nsInitialize", "nsAppNew", "nsAppInit", "nsAppNext")
}

NX_STUB_FN(service) {
	NX_STUB_NAMES("serviceInit", "serviceNew")
}

NX_STUB_FN(swkbd) {
	(void)iso;
	(void)init_obj;
}

// WebGL2 is EGL/Mesa over the Switch GPU — Switch-only. webglContextNew
// returns undefined so `screen.getContext('webgl2')` yields null on host;
// webglInitClass is a no-op (the TS class still installs its constants).
NX_STUB_FN(webgl) {
	static const char *const names[] = {"webglContextNew", "webglInitClass"};
	nx_stub_register(iso, init_obj, names, countof(names), nx_stub_undefined);
}

NX_STUB_FN(web) {
	NX_STUB_NAMES("webAppletNew", "webAppletStart", "webAppletAppear",
	              "webAppletSendMessage", "webAppletPollMessages",
	              "webAppletRequestExit", "webAppletClose",
	              "webAppletIsRunning", "webAppletGetMode")
}
