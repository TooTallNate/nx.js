#pragma once
#include "poll.h"
#include "thpool.h"
#include <cairo-ft.h>
#include <ft2build.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <poll.h>
#include <pthread.h>
#include <quickjs.h>
#include <stdbool.h>
#include <switch.h>
#include <wasm3.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef countof
#define countof(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef NXJS_VERSION
#define NXJS_VERSION "0.0.0"
#endif

// Useful for functions defined on class `prototype`s
#define JS_PROP_C_W (JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE)

#define NX_DEF_GET_(THISARG, NAME, FN, FLAGS)                                  \
	atom = JS_NewAtom(ctx, NAME);                                              \
	JS_DefinePropertyGetSet(ctx, THISARG, atom,                                \
							JS_NewCFunction(ctx, FN, "get " NAME, 0), JS_NULL, \
							FLAGS);                                            \
	JS_FreeAtom(ctx, atom);

#define NX_DEF_GET(THISARG, NAME, FN)                                          \
	NX_DEF_GET_(THISARG, NAME, FN, JS_PROP_C_W)

#define NX_DEF_GETSET(THISARG, NAME, GET_FN, SET_FN)                           \
	atom = JS_NewAtom(ctx, NAME);                                              \
	JS_DefinePropertyGetSet(                                                   \
		ctx, THISARG, atom, JS_NewCFunction(ctx, GET_FN, "get " NAME, 0),      \
		JS_NewCFunction(ctx, SET_FN, "set " NAME, 0), JS_PROP_C_W);            \
	JS_FreeAtom(ctx, atom);

#define NX_DEF_FUNC(THISARG, NAME, FN, LENGTH)                                 \
	(JS_DefinePropertyValueStr(ctx, THISARG, NAME,                             \
							   JS_NewCFunction(ctx, FN, NAME, LENGTH),         \
							   JS_PROP_C_W))

typedef struct {
	JSContext *context;
	JSValue callback;
	JSValue buffer;
} nx_js_callback_t;

typedef struct nx_work_s nx_work_t;
typedef void (*nx_work_cb)(nx_work_t *req);
typedef JSValue (*nx_after_work_cb)(JSContext *ctx, nx_work_t *req);

struct nx_work_s {
	nx_work_t *next;
	int done;
	JSValue resolve;
	JSValue reject;
	nx_work_cb work_cb;
	nx_after_work_cb after_work_cb;
	pthread_mutex_t *async_done_mutex;
	void *data;
};

enum nx_rendering_mode { NX_RENDERING_MODE_CONSOLE, NX_RENDERING_MODE_CANVAS };

typedef struct nx_context_s {
	int had_error;
	enum nx_rendering_mode rendering_mode;
	nx_poll_t poll;
	threadpool thpool;
	pthread_mutex_t async_done_mutex;
	nx_work_t *work_queue;
	FT_Library ft_library;
	HidVibrationDeviceHandle vibration_device_handles[2];
	IM3Environment wasm_env;
	JSValue init_obj;
	JSValue frame_handler;
	JSValue exit_handler;
	JSValue error_handler;
	JSValue unhandled_rejection_handler;
	PadState pads[8];

	// mbedtls structures shared by all TLS connections
	bool mbedtls_initialized;
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;

	bool spl_initialized;
} nx_context_t;
