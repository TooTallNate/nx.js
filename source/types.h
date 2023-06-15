#pragma once
#include <pthread.h>
#include <quickjs/quickjs.h>
#include <cairo-ft.h>
#include <ft2build.h>
#include "thpool.h"

#ifndef countof
#define countof(x) (sizeof(x) / sizeof((x)[0]))
#endif

typedef int BOOL;

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
    pthread_mutex_t* async_done_mutex;
    void *data;
};

typedef struct
{
    threadpool thpool;
    pthread_mutex_t async_done_mutex;
    nx_work_t *work_queue;
    FT_Library ft_library;
} nx_context_t;

inline nx_context_t *nx_get_context(JSContext *ctx)
{
    return JS_GetContextOpaque(ctx);
}

// DANGER: Internal Promise state from `quickjs.c`.
// Verify that this is still accurate if/when QuickJS
// ever gets updated.
#define JS_CLASS_PROMISE 49

struct list_head {
    struct list_head *prev;
    struct list_head *next;
};

typedef enum JSPromiseStateEnum {
    JS_PROMISE_PENDING,
    JS_PROMISE_FULFILLED,
    JS_PROMISE_REJECTED,
} JSPromiseStateEnum;

typedef struct JSPromiseData {
    JSPromiseStateEnum promise_state;
    /* 0=fulfill, 1=reject, list of JSPromiseReactionData.link */
    struct list_head promise_reactions[2];
    BOOL is_handled; /* Note: only useful to debug */
    JSValue promise_result;
} JSPromiseData;
