#include "async.h"
#include "error.h"
#include <stdlib.h>

void nx_process_async(JSContext *ctx, nx_context_t *nx_ctx) {
	nx_work_t *prev = NULL;
	nx_work_t *cur = nx_ctx->work_queue;
	pthread_mutex_lock(&nx_ctx->async_done_mutex);
	while (cur != NULL) {
		if (cur->done) {
			nx_work_t *next = cur->next;
			JSValue result = cur->after_work_cb(ctx, cur);
			JSValue ret_val;
			JSValue args[1];
			if (JS_IsException(result)) {
				args[0] = JS_GetException(ctx);
				ret_val = JS_Call(ctx, cur->reject, JS_NULL, 1, args);
			} else {
				args[0] = result;
				ret_val = JS_Call(ctx, cur->resolve, JS_NULL, 1, args);
			}
			JS_FreeValue(ctx, args[0]);
			JS_FreeValue(ctx, cur->resolve);
			JS_FreeValue(ctx, cur->reject);
			if (JS_IsException(ret_val)) {
				nx_emit_error_event(ctx);
			}
			JS_FreeValue(ctx, ret_val);
			if (cur->data)
				free(cur->data);
			free(cur);

			cur = next;

			// remove entry from linked list
			if (prev == NULL) {
				// at the start of the list, so reset the context pointer
				nx_ctx->work_queue = cur;
			} else {
				prev->next = cur;
			}

			// If the callback threw a fatal error
			// then don't process any more async callbacks
			if (nx_ctx->had_error)
				break;
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

JSValue nx_queue_async(JSContext *ctx, nx_work_t *req, nx_work_cb work_cb,
					   nx_after_work_cb after_work_cb) {
	JSValue promise, resolving_funcs[2];
	promise = JS_NewPromiseCapability(ctx, resolving_funcs);
	req->done = 0;
	req->resolve = resolving_funcs[0];
	req->reject = resolving_funcs[1];
	req->work_cb = work_cb;
	req->after_work_cb = after_work_cb;

	// Add to linked list
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	pthread_mutex_lock(&nx_ctx->async_done_mutex);
	if (nx_ctx->work_queue) {
		req->next = nx_ctx->work_queue;
	}
	nx_ctx->work_queue = req;
	pthread_mutex_unlock(&nx_ctx->async_done_mutex);

	if (thpool_add_work(nx_ctx->thpool, (void (*)(void *))nx_do_async, req) !=
		0) {
		// TODO: throw exception / clean up
	}

	return promise;
}
