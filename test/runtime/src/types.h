/* Minimal types.h shim for url.c in test harness */
#pragma once
#include <quickjs.h>
#include <stdbool.h>
#include <stdint.h>

typedef uint32_t u32;

#ifndef countof
#define countof(x) (sizeof(x) / sizeof((x)[0]))
#endif

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
