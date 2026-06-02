#pragma once
#include "types.h"

// Native-handle wrapping for JS objects (replaces QuickJS class-id + opaque +
// JSClassDef.finalizer). See BINDINGS.md §4–§5.
//
// Usage:
//   // 1. Make an object that can hold a native pointer (1 internal field):
//   v8::Local<v8::Object> obj = nx_new_wrapped(iso);
//   // 2. Attach the pointer + a GC finalizer that frees it:
//   nx_wrap(iso, obj, ptr, [](MyType *p){ /* free p */ });
//   // 3. Retrieve later (from `this` / an argument):
//   MyType *p = nx_unwrap<MyType>(obj);

namespace nx {

// A per-isolate cached ObjectTemplate with a single internal field. Lazily
// created. Used to mint objects that can carry a native pointer.
v8::Local<v8::Object> NewWrapped(v8::Isolate *iso);

namespace detail {
template <typename T> struct WeakRef {
	v8::Global<v8::Object> handle;
	T *ptr;
	void (*free_fn)(T *);
};
} // namespace detail

// Attach `ptr` to `obj`'s internal field 0 and register a GC finalizer that
// calls `free_fn(ptr)` when `obj` is collected. The finalizer must NOT touch
// any V8 API (it runs during GC).
template <typename T>
void Wrap(v8::Isolate *iso, v8::Local<v8::Object> obj, T *ptr,
          void (*free_fn)(T *)) {
	obj->SetAlignedPointerInInternalField(0, ptr,
	                                      v8::kEmbedderDataTypeTagDefault);
	auto *ref = new detail::WeakRef<T>{v8::Global<v8::Object>(iso, obj), ptr,
	                                   free_fn};
	ref->handle.SetWeak(
	    ref,
	    [](const v8::WeakCallbackInfo<detail::WeakRef<T>> &data) {
		    detail::WeakRef<T> *r = data.GetParameter();
		    if (r->free_fn && r->ptr) {
			    r->free_fn(r->ptr);
		    }
		    r->handle.Reset();
		    delete r;
	    },
	    v8::WeakCallbackType::kParameter);
}

// Retrieve the native pointer from internal field 0 (or nullptr).
template <typename T> T *Unwrap(v8::Local<v8::Value> obj) {
	if (obj.IsEmpty() || !obj->IsObject()) {
		return nullptr;
	}
	v8::Local<v8::Object> o = obj.As<v8::Object>();
	if (o->InternalFieldCount() < 1) {
		return nullptr;
	}
	return static_cast<T *>(o->GetAlignedPointerFromInternalField(
	    0, v8::kEmbedderDataTypeTagDefault));
}

} // namespace nx
