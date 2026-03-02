/**
 * Stub implementations for the nxjs-test host binary.
 *
 * Provides:
 * 1. No-op nx_init_* for Switch-only modules (account, album, applet, audio,
 *    battery, fsdev, gamepad, irs, nifm, ns, service, software-keyboard)
 * 2. Stubs for libnx functions/symbols referenced by main.c that are
 *    Switch-specific (console, framebuffer, loading image, etc.)
 */

#include "types.h"
#include <quickjs.h>

// ============================================================================
// No-op nx_init_* for Switch-only modules
// ============================================================================

// These modules are entirely Switch-specific and have no host implementation.
// Their nx_init_* functions would normally register native bindings on the
// $ init object. Here they do nothing — the runtime.js code will still create
// the JS-side classes, but any native methods will be caught by the Proxy stub
// (if used) or simply not be registered.

void nx_init_account(JSContext *ctx, JSValueConst init_obj) {
	(void)ctx;
	(void)init_obj;
}

void nx_init_album(JSContext *ctx, JSValueConst init_obj) {
	(void)ctx;
	(void)init_obj;
}

void nx_init_applet(JSContext *ctx, JSValueConst init_obj) {
	(void)ctx;
	(void)init_obj;
}

void nx_init_audio(JSContext *ctx, JSValueConst init_obj) {
	(void)ctx;
	(void)init_obj;
}

void nx_init_battery(JSContext *ctx, JSValueConst init_obj) {
	(void)ctx;
	(void)init_obj;
}

void nx_init_fs(JSContext *ctx, JSValueConst init_obj) {
	(void)ctx;
	(void)init_obj;
}

void nx_init_fsdev(JSContext *ctx, JSValueConst init_obj) {
	(void)ctx;
	(void)init_obj;
}

void nx_init_gamepad(JSContext *ctx, JSValueConst init_obj) {
	(void)ctx;
	(void)init_obj;
}

void nx_init_irs(JSContext *ctx, JSValueConst init_obj) {
	(void)ctx;
	(void)init_obj;
}

void nx_init_nifm(JSContext *ctx, JSValueConst init_obj) {
	(void)ctx;
	(void)init_obj;
}

void nx_init_ns(JSContext *ctx, JSValueConst init_obj) {
	(void)ctx;
	(void)init_obj;
}

void nx_init_service(JSContext *ctx, JSValueConst init_obj) {
	(void)ctx;
	(void)init_obj;
}

void nx_init_swkbd(JSContext *ctx, JSValueConst init_obj) {
	(void)ctx;
	(void)init_obj;
}
