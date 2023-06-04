#ifndef _NX_TYPES_
#define _NX_TYPES_

#include <stdbool.h>
#include <quickjs/quickjs.h>
#include "list.h"
#include "thpool.h"

typedef struct
{
    nx_list_t list;
    int done;
    void (*callback)(void *);
    JSContext *ctx;
    JSValue js_callback;
    pthread_mutex_t *async_done_mutex;
} nx_async_result_t;

typedef struct
{
    threadpool thpool;
    pthread_mutex_t async_done_mutex;
    nx_async_result_t *work;
} nx_context_t;

#endif