#include "async.h"
#include "error.h"
#include <stdlib.h>

using namespace v8;

// Runs on a libuv worker thread. NO V8 API allowed here.
static void nx_uv_work_cb(uv_work_t *uvreq) {
	nx_work_t *req = reinterpret_cast<nx_work_t *>(uvreq);
	req->work_cb(req);
}

// Runs on the loop thread after the worker finishes. V8 API allowed.
static void nx_uv_after_work_cb(uv_work_t *uvreq, int status) {
	nx_work_t *req = reinterpret_cast<nx_work_t *>(uvreq);
	Isolate *iso = req->iso;
	nx_context_t *ctx = nx_ctx(iso);

	{
		Isolate::Scope iso_scope(iso);
		HandleScope scope(iso);
		Local<Context> context = iso->GetCurrentContext();
		Context::Scope ctx_scope(context);

		Local<Promise::Resolver> resolver = req->resolver.Get(iso);

		TryCatch try_catch(iso);
		MaybeLocal<Value> maybe_result = req->after_work_cb(iso, req);

		if (try_catch.HasCaught() || maybe_result.IsEmpty()) {
			// after_work_cb left an exception pending -> reject.
			Local<Value> err = try_catch.Exception();
			if (err.IsEmpty()) {
				err = Exception::Error(nx_str(iso, "async operation failed"));
			}
			resolver->Reject(context, err).Check();
		} else {
			resolver->Resolve(context, maybe_result.ToLocalChecked()).Check();
		}
	}

	req->resolver.Reset();
	if (req->data) {
		if (req->data_dtor) {
			req->data_dtor(req->data);
		} else {
			free(req->data);
		}
	}
	delete req;

	// Drain microtasks scheduled by the resolve/reject (the loop also does
	// this each frame, but doing it here keeps short async chains snappy).
	iso->PerformMicrotaskCheckpoint();

	(void)ctx;
	(void)status;
}

Local<Promise> nx_queue_async(Isolate *iso, nx_work_t *req, nx_work_cb work_cb,
                              nx_after_work_cb after_work_cb) {
	Local<Context> context = iso->GetCurrentContext();
	Local<Promise::Resolver> resolver =
	    Promise::Resolver::New(context).ToLocalChecked();

	req->iso = iso;
	req->resolver.Reset(iso, resolver);
	req->work_cb = work_cb;
	req->after_work_cb = after_work_cb;
	req->failed = false;

	nx_context_t *ctx = nx_ctx(iso);
	uv_queue_work(ctx->loop, &req->req, nx_uv_work_cb, nx_uv_after_work_cb);

	return resolver->GetPromise();
}
