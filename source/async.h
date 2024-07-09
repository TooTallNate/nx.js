#pragma once
#include "types.h"

#define NX_INIT_WORK_T(type)                                                   \
	nx_work_t *req = malloc(sizeof(nx_work_t));                                \
	memset(req, 0, sizeof(nx_work_t));                                         \
	type *data = malloc(sizeof(type));                                         \
	memset(data, 0, sizeof(type));                                             \
	req->data = data;

void nx_process_async(JSContext *ctx, nx_context_t *nx_ctx);
JSValue nx_queue_async(JSContext *ctx, nx_work_t *req, nx_work_cb work_cb,
					   nx_after_work_cb after_work_cb);
