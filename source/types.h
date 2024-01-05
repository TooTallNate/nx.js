#pragma once
#include <stdbool.h>
#include <wasm3.h>
#include <pthread.h>
#include <quickjs.h>
#include <cairo-ft.h>
#include <ft2build.h>
#include <poll.h>
#include <switch.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include "thpool.h"
#include "poll.h"

#ifndef countof
#define countof(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef NXJS_VERSION
#define NXJS_VERSION "0.0.0"
#endif

// Useful for functions defined on class `prototype`s
#define JS_PROP_C_W (JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE)

#define NX_DEF_GET(THISARG, NAME, FN)                                                                            \
	atom = JS_NewAtom(ctx, NAME);                                                                                \
	JS_DefinePropertyGetSet(ctx, THISARG, atom, JS_NewCFunction(ctx, FN, "get " NAME, 0), JS_NULL, JS_PROP_C_W); \
	JS_FreeAtom(ctx, atom);

#define NX_DEF_SET(THISARG, NAME, FN)                                                                            \
	atom = JS_NewAtom(ctx, NAME);                                                                                \
	JS_DefinePropertyGetSet(ctx, THISARG, atom, JS_NULL, JS_NewCFunction(ctx, FN, "set " NAME, 0), JS_PROP_C_W); \
	JS_FreeAtom(ctx, atom);

#define NX_DEF_GETSET(THISARG, NAME, GET_FN, SET_FN)                                                                                                      \
	atom = JS_NewAtom(ctx, NAME);                                                                                                                         \
	JS_DefinePropertyGetSet(ctx, THISARG, atom, JS_NewCFunction(ctx, GET_FN, "get " NAME, 0), JS_NewCFunction(ctx, SET_FN, "set " NAME, 0), JS_PROP_C_W); \
	JS_FreeAtom(ctx, atom);

#define NX_DEF_FUNC(THISARG, NAME, FN, LENGTH) (JS_DefinePropertyValueStr(ctx, THISARG, NAME, JS_NewCFunction(ctx, FN, NAME, LENGTH), JS_PROP_C_W))

typedef struct
{
	JSContext *context;
	JSValue callback;
	JSValue buffer;
} nx_js_callback_t;

typedef struct nx_work_s nx_work_t;
typedef void (*nx_work_cb)(nx_work_t *req);
typedef void (*nx_after_work_cb)(JSContext *ctx, nx_work_t *req, JSValue *args);

struct nx_work_s
{
	nx_work_t *next;
	int done;
	JSValue js_callback;
	nx_work_cb work_cb;
	nx_after_work_cb after_work_cb;
	pthread_mutex_t *async_done_mutex;
	void *data;
};

enum nx_rendering_mode
{
	NX_RENDERING_MODE_CONSOLE,
	NX_RENDERING_MODE_CANVAS
};

typedef struct nx_context_s
{
	int had_error;
	enum nx_rendering_mode rendering_mode;
	nx_poll_t poll;
	threadpool thpool;
	pthread_mutex_t async_done_mutex;
	nx_work_t *work_queue;
	FT_Library ft_library;
	HidVibrationDeviceHandle vibration_device_handles[2];
	IM3Environment wasm_env;
	JSValue frame_handler;
	JSValue exit_handler;
	JSValue error_handler;
	JSValue unhandled_rejection_handler;

	// mbedtls structures shared by all TLS connections
	bool mbedtls_initialized;
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
} nx_context_t;
