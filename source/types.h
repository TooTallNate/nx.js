#ifndef _NX_TYPES_
#define _NX_TYPES_

#include <pthread.h>
#include <quickjs/quickjs.h>
#include "thpool.h"

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
} nx_context_t;

#endif