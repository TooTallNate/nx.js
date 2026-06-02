#include "error.h"
#include <stdio.h>
#include <string.h>

using namespace v8;

// Extract the `.stack` (or String(value)) of an exception value as UTF-8.
static void print_exception_value(Isolate *iso, Local<Context> context,
                                  Local<Value> exception, const char *prefix) {
	String::Utf8Value exception_str(iso, exception);
	fprintf(stderr, "%s%s\n", prefix,
	        *exception_str ? *exception_str : "<exception>");

	if (exception->IsObject()) {
		Local<Value> stack;
		if (exception.As<Object>()
		        ->Get(context, nx_str(iso, "stack"))
		        .ToLocal(&stack) &&
		    !stack->IsUndefined()) {
			String::Utf8Value stack_str(iso, stack);
			if (*stack_str) {
				fprintf(stderr, "%s\n", *stack_str);
			}
		}
	}
}

void print_js_error(Isolate *iso, TryCatch *try_catch) {
	HandleScope scope(iso);
	Local<Context> context = iso->GetCurrentContext();
	Local<Value> exception = try_catch->Exception();
	String::Utf8Value exception_str(iso, exception);
	printf("%s\n", *exception_str ? *exception_str : "<exception>");
	print_exception_value(iso, context, exception, "");
}

void nx_throw(Isolate *iso, const char *message) {
	iso->ThrowException(Exception::Error(nx_str(iso, message)));
}

void nx_throw_libnx_error(Isolate *iso, Result rc, const char *name) {
	HandleScope scope(iso);
	Local<Context> context = iso->GetCurrentContext();
	u32 module = R_MODULE(rc);
	u32 desc = R_DESCRIPTION(rc);
	char message[1024];
	snprintf(message, sizeof(message),
	         "%s failed (module: %u, description: %u)", name, module, desc);
	Local<Object> err =
	    Exception::Error(nx_str(iso, message)).As<Object>();
	err->Set(context, nx_str(iso, "module"), Integer::NewFromUnsigned(iso, module))
	    .Check();
	err->Set(context, nx_str(iso, "description"),
	         Integer::NewFromUnsigned(iso, desc))
	    .Check();
	err->Set(context, nx_str(iso, "value"),
	         Integer::NewFromUnsigned(iso, R_VALUE(rc)))
	    .Check();
	iso->ThrowException(err);
}

void nx_throw_errno_error(Isolate *iso, int err, const char *syscall) {
	HandleScope scope(iso);
	Local<Context> context = iso->GetCurrentContext();
	char message[1024];
	snprintf(message, sizeof(message), "%s (%s)", strerror(err), syscall);
	Local<Object> error =
	    Exception::Error(nx_str(iso, message)).As<Object>();
	error->Set(context, nx_str(iso, "errno"), Integer::New(iso, err)).Check();
	iso->ThrowException(error);
}

void nx_emit_error_event(Isolate *iso, TryCatch *try_catch) {
	HandleScope scope(iso);
	Local<Context> context = iso->GetCurrentContext();
	nx_context_t *ctx = nx_ctx(iso);

	Local<Value> exception = try_catch->Exception();
	print_exception_value(iso, context, exception, "Uncaught ");

	if (ctx->error_handler.IsEmpty()) {
		ctx->had_error = 1;
		return;
	}

	Local<Function> handler = ctx->error_handler.Get(iso);
	Local<Value> args[] = {exception};
	TryCatch inner(iso);
	Local<Value> ret;
	if (!handler->Call(context, Null(iso), 1, args).ToLocal(&ret)) {
		print_js_error(iso, &inner);
		return;
	}
	int had_error = 0;
	if (ret->Int32Value(context).To(&had_error)) {
		ctx->had_error = had_error;
	}
}

static void nx_set_error_handler(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_ctx(iso)->error_handler.Reset(iso, info[0].As<Function>());
}

static void nx_set_unhandled_rejection_handler(
    const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_ctx(iso)->unhandled_rejection_handler.Reset(iso,
	                                               info[0].As<Function>());
}

void nx_promise_rejection_handler(PromiseRejectMessage message) {
	Isolate *iso = Isolate::GetCurrent();
	nx_context_t *ctx = nx_ctx(iso);
	Local<Promise> promise = message.GetPromise();

	switch (message.GetEvent()) {
	case kPromiseRejectWithNoHandler:
		// A promise was rejected without a handler. Remember it; if a handler
		// is attached later, kPromiseHandlerAddedAfterReject clears it.
		ctx->unhandled_rejected_promise.Reset(iso, promise);
		break;
	case kPromiseHandlerAddedAfterReject:
		if (!ctx->unhandled_rejected_promise.IsEmpty()) {
			Local<Promise> stored = ctx->unhandled_rejected_promise.Get(iso);
			if (stored == promise) {
				ctx->unhandled_rejected_promise.Reset();
			}
		}
		break;
	case kPromiseRejectAfterResolved:
	case kPromiseResolveAfterResolved:
		break;
	}
}

void nx_emit_unhandled_rejection_event(Isolate *iso) {
	HandleScope scope(iso);
	Local<Context> context = iso->GetCurrentContext();
	nx_context_t *ctx = nx_ctx(iso);

	if (ctx->unhandled_rejected_promise.IsEmpty()) {
		return;
	}
	Local<Promise> promise = ctx->unhandled_rejected_promise.Get(iso);
	Local<Value> reason = promise->Result();

	print_exception_value(iso, context, reason, "Uncaught (in promise) ");

	if (!ctx->unhandled_rejection_handler.IsEmpty()) {
		Local<Function> handler = ctx->unhandled_rejection_handler.Get(iso);
		Local<Value> args[] = {promise, reason};
		TryCatch inner(iso);
		Local<Value> ret;
		if (!handler->Call(context, Null(iso), 2, args).ToLocal(&ret)) {
			print_js_error(iso, &inner);
		} else {
			int had_error = 0;
			if (ret->Int32Value(context).To(&had_error)) {
				ctx->had_error = had_error;
			}
		}
	}

	ctx->unhandled_rejected_promise.Reset();
}

void nx_init_error(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "onError", nx_set_error_handler);
	NX_SET_FUNC(init_obj, "onUnhandledRejection",
	            nx_set_unhandled_rejection_handler);
}
