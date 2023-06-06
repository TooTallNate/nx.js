#include "async.h"

void nx_process_async(JSContext *ctx, nx_context_t *nx_ctx)
{
    nx_work_t *prev = NULL;
    nx_work_t *cur = nx_ctx->work_queue;
    pthread_mutex_lock(&nx_ctx->async_done_mutex);
    while (cur != NULL) {
        if (cur->done) {
            nx_work_t *next = cur->next;
            cur->after_work_cb(ctx, cur);
            cur = next;

            // remove entry from linked list
            if (prev == NULL) {
                // at the start of the list, so reset the context pointer
                nx_ctx->work_queue = cur;
            } else {
                prev->next = cur;
            }
        } else {
            prev = cur;
            cur = cur->next;
        }
    }
    pthread_mutex_unlock(&nx_ctx->async_done_mutex);
}

void nx_do_async(nx_work_t *req) {
    req->work_cb(req);
    pthread_mutex_lock(req->async_done_mutex);
    req->done = 1;
    pthread_mutex_unlock(req->async_done_mutex);
}

int nx_queue_async(JSContext *ctx, nx_work_t *req, nx_work_cb work_cb, nx_after_work_cb after_work_cb, JSValueConst js_callback)
{
    nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
    req->done = 0;
    req->js_callback = JS_DupValue(ctx, js_callback);
    req->work_cb = work_cb;
    req->after_work_cb = after_work_cb;

    // Add to linked list
    if (nx_ctx->work_queue) {
        req->next = nx_ctx->work_queue;
    }
    nx_ctx->work_queue = req;

    return thpool_add_work(nx_ctx->thpool, (void (*)(void *))nx_do_async, req);
}
