#include "async.h"
#include "error.h"
#include <malloc.h>
#include <stdlib.h>

using namespace v8;

// newlib heap bounds: the process's total native malloc capacity.
extern "C" char *fake_heap_start;
extern "C" char *fake_heap_end;

// Native-heap back-pressure for high-rate async allocators.
//
// Async ops that hand large ArrayBuffers back to JS (zstd/zlib decompression,
// image decode, file reads) allocate their result OFF the V8 heap as external
// memory. V8's GC schedules around the *managed* heap, which stays tiny here,
// so during a tight async loop (e.g. reading a DecompressionStream chunk by
// chunk) it can let external backing stores pile up far faster than it
// collects them. On a memory-constrained applet (~30 MiB free native heap)
// that hits ENOMEM in seconds even though almost all of those buffers are
// already unreferenced garbage.
//
// After each async op completes, if the native heap is running low we fire a
// V8 MemoryPressureNotification(kCritical) — which performs a blocking GC and
// reclaims the unreferenced external backing stores — before returning to the
// loop to start the next op. mallinfo() is cheap but not free, so only sample
// it every few ops.
static void nx_async_relieve_native_pressure(Isolate *iso) {
	size_t total = (size_t)(fake_heap_end - fake_heap_start);
	if (total == 0)
		return;
	// Sampled on EVERY async completion. mallinfo() is a cheap arena walk
	// compared to the multi-MiB decompress/crypto/decode op that just ran, and
	// the producers spike fast enough (a single op can allocate several MiB of
	// result + intermediate buffers) that throttling the sample let transient
	// spikes overshoot the ceiling between checks. So check each time.
	struct mallinfo mi = mallinfo();
	size_t used = (size_t)mi.uordblks;
	size_t free_bytes = total > used ? total - used : 0;
	// Threshold: when under ~40 MiB of headroom remains, ask V8 to free now.
	// Generous enough that even a burst of large allocations within one loop
	// turn can't exhaust the heap before the next op's check fires. A no-op in
	// the application regime (always far more than 40 MiB free). kCritical
	// performs a blocking GC, reclaiming unreferenced external backing stores.
	if (free_bytes < 40ull * 1024 * 1024) {
		iso->MemoryPressureNotification(MemoryPressureLevel::kCritical);
	}
}

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
		// During teardown (Switch.exit / + / HOME mid-flight) the main loop's
		// Context::Scope has already been popped, so the isolate has no current
		// context. A queued work item's after-work callback can still fire here
		// (the exit path drains the loop with uv_run before disposing the
		// isolate), and entering an empty context would Data Abort @ 0x0 inside
		// v8::Context::Enter(). Fall back to the context captured on the work
		// request; if neither is available, the runtime is going away — drop the
		// result (free data + delete req below) instead of resolving.
		Local<Context> context = iso->GetCurrentContext();
		if (context.IsEmpty() && !req->context.IsEmpty()) {
			context = req->context.Get(iso);
		}
		if (context.IsEmpty()) {
			req->resolver.Reset();
			req->context.Reset();
			if (req->data) {
				if (req->data_dtor) {
					req->data_dtor(req->data);
				} else {
					free(req->data);
				}
			}
			delete req;
			return;
		}
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
	req->context.Reset();
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

	// If the native heap is running low (high-rate ArrayBuffer producers
	// outpacing GC), ask V8 to reclaim unreferenced external backing stores
	// now, before the loop starts the next op. See the helper above.
	nx_async_relieve_native_pressure(iso);

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
	// Capture the context the work was queued in, so the after-work callback
	// can still resolve/reject even if it fires after the main loop's
	// Context::Scope was popped during teardown (see nx_uv_after_work_cb).
	req->context.Reset(iso, context);
	req->work_cb = work_cb;
	req->after_work_cb = after_work_cb;
	req->failed = false;

	nx_context_t *ctx = nx_ctx(iso);
	uv_queue_work(ctx->loop, &req->req, nx_uv_work_cb, nx_uv_after_work_cb);

	return resolver->GetPromise();
}
