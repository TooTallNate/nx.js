# nx.js native binding conventions (V8 + libuv)

This is the **mandatory house style** for the native layer after the
QuickJS→V8 and hand-rolled-loop→libuv migration. Every C++ module in `source/`
follows these conventions so that 27 modules stay uniform despite using the raw
`v8::` API directly (no helper/abstraction layer — by design).

If you are porting an old QuickJS `.c` module, this file is the translation
table. Read it fully before touching a module.

> Scope note: this describes **Phase 1** (engine + loop). Cairo rendering is
> untouched in Phase 1; the canvas/font/image *bodies* stay as-is and only their
> JS-binding shells are converted. Skia (Phase 2) gets its own notes later.

---

## 0. Ground rules

- **Language**: every module is C++ (`.cc`), compiled `-std=c++20 -fno-rtti
  -fno-exceptions`. V8's API is C++ and the embedder links with `g++`. Do not
  use exceptions or RTTI anywhere (matches V8's ABI; the build will break
  otherwise).
- **No `V8_COMPRESS_POINTERS`**: never define it (V8 checks by *presence*; even
  `=0` aborts at `Initialize`). The switch-v8 monolith is built pointer-
  compression OFF.
- **One isolate, one context, single-threaded JS**: V8 runs
  `--single-threaded --single-threaded-gc --predictable`. All JS and all V8 API
  calls happen on the **main (loop) thread**. The only code allowed off-thread
  is libuv `uv_work_t` work callbacks (see §6) — which must touch **zero** V8
  API.
- **Headers**: V8 headers are flat — `#include <v8.h>`,
  `#include <libplatform/libplatform.h>`. libuv is `#include <uv.h>`.
- **Never hard-crash the console.** On a Switch a process abort forces the user
  to restart the whole console, so a binding must NEVER abort on bad/large input
  or allocation failure — it must surface a *catchable JS error* (or, for an
  unrecoverable VM state, set `nx_ctx->had_error` so the loop stops the VM and
  waits for `+`). Concretely:
  - **`.ToLocalChecked()` / `.Check()` / `.FromJust()` abort** if the
    Maybe/MaybeLocal is empty/Nothing. Only use them on values that are an
    internal invariant (a fresh `Object`/`Array`, a string *literal*, an
    init-time template). For anything derived from **user input or external
    data** — `obj->Get(...)` on a user object (a getter can throw), number
    coercions, `String::NewFromUtf8` on runtime bytes — use `.ToLocal(&out)` /
    `.To(&out)` and `return` on failure (the pending exception then surfaces as
    a normal throw).
  - For **UTF-8 from untrusted sources** (filenames, account nicknames, IPC
    payloads, URL components) use `nx_str_lossy()` — it falls back to a
    byte-for-byte string instead of aborting on malformed UTF-8. `nx_str()`
    (which is `NewFromUtf8(...).ToLocalChecked()`) is only for **string
    literals**.
  - For **allocations** use `nx_alloc(iso, size)` — it throws a `RangeError` and
    returns `NULL` on failure (caller must check + `return`). Never deref a bare
    `malloc`/`calloc` result without a NULL check. In a `uv_work_t` work
    callback (no V8 allowed) signal the failure via the request struct's error
    field and throw in the after-work callback instead.
  - KNOWN FOLLOW-UP: bare `new T()` (e.g. the `NX_INIT_WORK_T` macros) calls
    `std::terminate` on OOM under `-fno-exceptions`. These are tiny structs so
    failure is unlikely, but they should migrate to `new (std::nothrow)` +
    NULL-check. Tracked separately.

---

## 1. Module file layout

Each feature is `foo.cc` + `foo.h`. The header declares exactly one symbol:

```cpp
#pragma once
#include "types.h"
void nx_init_foo(v8::Isolate *iso, v8::Local<v8::Object> init_obj);
```

`main.cc` `#include`s the header and calls `nx_init_foo(iso, init_obj)` in the
init block (alphabetical), exactly as before.

The `.cc` skeleton:

```cpp
#include "foo.h"

namespace {  // file-local; never export anything but nx_init_foo

using namespace v8;

void nx_foo_do_thing(const FunctionCallbackInfo<Value> &info) {
    Isolate *iso = info.GetIsolate();
    HandleScope scope(iso);
    Local<Context> ctx = iso->GetCurrentContext();
    // ... implementation ...
    info.GetReturnValue().Set( /* result */ );
}

}  // namespace

void nx_init_foo(Isolate *iso, Local<Object> init_obj) {
    NX_SET_FUNC(init_obj, "fooDoThing", nx_foo_do_thing);
}
```

### Native function signature

The QuickJS `JSValue fn(JSContext*, JSValueConst this, int argc,
JSValueConst* argv)` becomes:

```cpp
void nx_foo(const v8::FunctionCallbackInfo<v8::Value> &info)
```

- Arguments: `info[0]`, `info[1]`, … (an out-of-range index yields `undefined`,
  same as QuickJS reading past `argc`). Count: `info.Length()`.
- `this`: `info.This()`. Holder: `info.Holder()`.
- Return: `info.GetReturnValue().Set(value)`. Returning nothing ⇒ `undefined`.
- **Always** open a `HandleScope` first (unless the function is trivial and
  creates no handles). Grab `ctx = iso->GetCurrentContext()` once.

---

## 2. Registering functions / the `$` object

QuickJS used `JSCFunctionListEntry function_list[]` + `JS_CFUNC_DEF` +
`JS_SetPropertyFunctionList`. We replace this with a single helper macro defined
in `types.h` (this is the *only* sanctioned macro; everything else is raw v8):

```cpp
// types.h
#define NX_SET_FUNC(OBJ, NAME, FN)                                        \
    do {                                                                  \
        v8::Local<v8::Context> _c = iso->GetCurrentContext();             \
        (OBJ)->Set(_c, nx_str(iso, NAME),                                 \
                   v8::FunctionTemplate::New(iso, FN)                     \
                       ->GetFunction(_c).ToLocalChecked())                \
            .Check();                                                     \
    } while (0)
```

`init_obj` is the `$` bridge object. Each `nx_init_*` attaches its native
functions onto it with `NX_SET_FUNC`. This is the entire JS↔C surface, exactly
as the QuickJS `$` was.

`nx_str(iso, "literal")` is the standard interned-string helper (see §8).

---

## 3. Defining getters / setters on a class prototype

The `*InitClass(TheClass)` pattern is preserved: TS defines a class with
`stub()` methods/accessors, then calls `$.fooInitClass(TheClass)`, and C
overwrites the prototype. QuickJS used `NX_DEF_GET` / `NX_DEF_GETSET` /
`NX_DEF_FUNC` against `proto`. The V8 equivalents (also in `types.h`):

```cpp
// Inside nx_foo_init_class(info): the class is info[0].
Local<Object> cls = info[0].As<Object>();
Local<Object> proto = cls->Get(ctx, nx_str(iso, "prototype"))
                          .ToLocalChecked().As<Object>();

NX_DEF_GET(proto, "charging", nx_battery_charging);          // getter only
NX_DEF_GETSET(proto, "width", nx_get_width, nx_set_width);   // getter+setter
NX_DEF_FUNC(proto, "fillRect", nx_fill_rect, 4);             // method
```

The macros wrap `proto->SetAccessorProperty(...)` (for get/set) and
`proto->Set(...)` with a `FunctionTemplate`. Accessor callbacks use the
`FunctionCallbackInfo` form (NOT the old `AccessorNameGetterCallback`) so the
same `nx_*` function signature is used everywhere — uniformity over idiom.

> Why `SetAccessorProperty` and not `Object::SetAccessor`: we are decorating an
> *existing* user-defined prototype object at runtime, not building an
> `ObjectTemplate`. `SetAccessorProperty` takes `Local<Function>` getters/
> setters, which is exactly what we can build from a `FunctionTemplate`.

---

## 4. Wrapping native handles on JS objects (replaces class-id + opaque)

QuickJS used `JS_NewClassID` + `JS_NewClass` + `JSClassDef{.finalizer}` +
`JS_SetOpaque`/`JS_GetOpaque2`. V8 replacement:

There are two cases.

### 4a. The JS object already exists (the common nx.js case)

Most nx.js classes do `proto($.fooNew(...), TheClass)` in TS: C returns a plain
object and TS re-prototypes it. We attach the native pointer via an **internal
field**, but a plain `Object::New` has zero internal fields. So `fooNew`
creates the object from a per-module **`ObjectTemplate` with 1 internal field**:

```cpp
// Stored once per module, as a v8::Eternal or in nx_context_t.
Local<ObjectTemplate> tmpl = ObjectTemplate::New(iso);
tmpl->SetInternalFieldCount(1);

// In nx_foo_new():
Local<Object> obj = tmpl->NewInstance(ctx).ToLocalChecked();
nx_foo_t *self = new nx_foo_t{...};
obj->SetAlignedPointerInInternalField(0, self);
nx_set_finalizer(iso, obj, self, nx_foo_finalize);  // §5
info.GetReturnValue().Set(obj);
```

Retrieve in methods:

```cpp
static nx_foo_t *nx_get_foo(Local<Object> obj) {
    return static_cast<nx_foo_t *>(obj->GetAlignedPointerFromInternalField(0));
}
```

`SetAlignedPointerInInternalField` requires a pointer aligned to ≥2 bytes
(always true for `new`/`malloc`). This replaces `JS_SetOpaque`. Always
null-check (`GetInternalField` count, or a tag) before deref — a method could be
called on the wrong receiver.

> This V8 build's `Set/GetAlignedPointerFromInternalField` take a trailing
> `v8::EmbedderDataTypeTag` argument; pass `v8::kEmbedderDataTypeTagDefault`.
> The shared helpers in `wrap.h` (`nx::NewWrapped`/`nx::Wrap`/`nx::Unwrap`)
> already handle this — prefer them over calling the raw API per module.

### 4b. Pure data classes

Same as 4a; there is no separate "native class" registration step in V8. The
`ObjectTemplate` *is* the class shape.

---

## 5. Finalizers (freeing native state on GC)

QuickJS `JSClassDef.finalizer` → V8 weak callbacks. Standard helper:

```cpp
// Holds enough to free: the persistent handle + the native ptr.
template <typename T>
struct nx_weak_ref {
    v8::Global<v8::Object> handle;
    T *ptr;
    void (*free_fn)(T *);
};

template <typename T>
void nx_set_finalizer(v8::Isolate *iso, v8::Local<v8::Object> obj, T *ptr,
                      void (*free_fn)(T *)) {
    auto *ref = new nx_weak_ref<T>{v8::Global<v8::Object>(iso, obj), ptr, free_fn};
    ref->handle.SetWeak(ref, [](const v8::WeakCallbackInfo<nx_weak_ref<T>> &d) {
        nx_weak_ref<T> *r = d.GetParameter();
        r->free_fn(r->ptr);
        r->handle.Reset();
        delete r;
    }, v8::WeakCallbackType::kParameter);
}
```

Rules:
- The weak callback runs during GC. It **must not** allocate JS handles or call
  back into JS. Only free native memory (close fds, `cairo_destroy`, etc.).
- If a resource needs deterministic teardown (sockets, files), expose an
  explicit `fooClose()` that frees and **marks the struct closed**; the
  finalizer then becomes idempotent (no double-free).
- `WeakCallbackType::kParameter` (not `kInternalFields`) keeps it simple and
  works whether or not the object has internal fields.

---

## 6. Async / threadpool — now over libuv

The QuickJS model (custom `thpool` + a `work_queue` linked list drained each
frame in `nx_process_async`) is **replaced by libuv** (`uv_work_t` +
`uv_queue_work`). The "create a Promise now, resolve it later from the loop
thread" shape is preserved.

### The request struct

```cpp
struct nx_work_t {
    uv_work_t req;                      // first member; uv gives it back to us
    v8::Isolate *iso;
    v8::Global<v8::Promise::Resolver> resolver;
    // module-specific input/output below:
    void *data;
    bool failed;                        // set by work_cb to reject
    // ... result fields ...
};
```

### Queue helper (in `async.cc`)

```cpp
v8::Local<v8::Promise> nx_queue_async(v8::Isolate *iso, nx_work_t *w,
                                      uv_work_cb work_cb,
                                      void (*after)(nx_work_t *));
```

It:
1. Creates `Promise::Resolver::New(ctx)`, stores it as a `Global` on `w`.
2. Stores `after` (our typed after-cb) on `w`.
3. `uv_queue_work(nx_loop(), &w->req, work_cb, nx_after_work_trampoline)`.
4. Returns `resolver->GetPromise()` to JS synchronously.

`nx_after_work_trampoline(uv_work_t* req, int status)` runs on the **loop
thread**: it opens a `HandleScope` + `Context::Scope`, calls our `after(w)`
which fills the resolve/reject value, then `resolver->Resolve(...)` or
`->Reject(...)`, disposes the `Global`, and `delete w`.

### Rules (critical)

- **`work_cb` runs on a uv worker thread → NO V8 API, NO JS handles.** Pure C
  work only (read/write fds, mbedtls, decode, etc.). Read inputs from `w`,
  write outputs to `w`.
- **`after` runs on the loop thread → V8 API allowed.** Build the result handle
  here.
- Keep any `ArrayBuffer` backing memory alive across the threads by owning it in
  `w` (copy in, or hold the `BackingStore` shared_ptr), never by holding a bare
  `Local`.
- Microtasks scheduled by the resolve/reject run at the next checkpoint (§7),
  not inside the trampoline.

This deletes `thpool.c`, `poll.c`, and the bespoke `work_queue` drain entirely.

---

## 7. The event loop (libuv-hosted)

`main.cc` owns one loop iteration (per frame). Order:

```
uv_run(loop, UV_RUN_NOWAIT);            // sockets, fs, dns, uv_work_t afters, timers
isolate->PerformMicrotaskCheckpoint();  // promise reactions (replaces JS_ExecutePendingJob pump)
<call onFrame Global<Function>>;        // rAF + input dispatch
appletMainLoop();                       // OS messages; break loop on false
<present>;                              // framebuffer/EGL swap — BLOCKS at vsync (frame pacer)
```

- libuv runs **NOWAIT** so it never blocks the frame; the present is the pacer
  (`nwindowDequeueBuffer` blocks on vsync — verified in libnx).
- `setTimeout`/`setInterval` are backed by `uv_timer_t` (a `$` binding), not
  `Date.now()` polling.
- V8 foreground/delayed platform tasks are posted into the uv loop via a
  `uv_async_t` (wakeup) + a queue; the single-threaded `DefaultPlatform`'s
  task-runner is pumped from the same loop. (See `main.cc` / `platform.cc`.)
- Retained handlers (`onFrame`, `onExit`, `onError`, rejection) are
  `v8::Global<v8::Function>` on `nx_context_t` (replaces the retained
  `JSValue`s).

---

## 8. Common value marshalling cheatsheet (QuickJS → V8)

Always have `Isolate *iso` and `Local<Context> ctx` in scope.

| QuickJS | V8 |
|---|---|
| `JS_NewBool(ctx, b)` | `Boolean::New(iso, b)` |
| `JS_NewInt32/NewInt64` | `Integer::New(iso, n)` / `BigInt::New` |
| `JS_NewFloat64(ctx, d)` | `Number::New(iso, d)` |
| `JS_NewString(ctx, s)` | `nx_str(iso, s)` (literals only — aborts on bad UTF-8); for untrusted bytes use `nx_str_lossy(iso, s)` |
| `JS_NewStringLen` | `String::NewFromUtf8(iso, p, kNormal, len)` + `.ToLocal()`; for untrusted bytes `nx_str_lossy(iso, p, len)` |
| `JS_ToInt32(ctx,&i,v)` | `v->Int32Value(ctx).To(&i)` (check bool) |
| `JS_ToFloat64` | `v->NumberValue(ctx).To(&d)` |
| `JS_ToCStringLen` | `String::Utf8Value u(iso, v);` → `*u`, `u.length()` |
| `JS_IsException` / `JS_EXCEPTION` | a `MaybeLocal`/`Maybe` came back empty; or check `TryCatch` |
| `JS_ThrowTypeError(ctx,...)` | `iso->ThrowException(Exception::TypeError(nx_str(iso,msg)))` then `return` |
| `JS_ThrowInternalError` | `iso->ThrowException(Exception::Error(...))` (no "internal" variant) |
| `JS_GetPropertyStr(ctx,o,"k")` | `o->Get(ctx, nx_str(iso,"k")).ToLocal(&out)` then `return` on false (a user getter can throw; never `.ToLocalChecked()` on a user object) |
| `JS_SetPropertyStr` | `o->Set(ctx, nx_str(iso,"k"), v).Check()` |
| `JS_Call(ctx,fn,this,n,argv)` | `fn->Call(ctx, recv, n, argv)` (returns `MaybeLocal`) |
| `JS_GetContextOpaque(ctx)` | `nx_ctx(iso)` → `static_cast<nx_context_t*>(iso->GetData(0))` |
| `js_malloc`/`js_free` | `nx_alloc(iso, size)` (throws RangeError + returns NULL on failure) / `free`; bare `malloc` only if you NULL-check |

### C++ strictness vs the old C modules

These C→C++ gotchas recur when porting:
- **Enum args**: libnx setters take typed enums (e.g. `WebBootDisplayKind`),
  not `int`. The old C passed ints implicitly; C++ requires an explicit cast:
  `webConfigSetBootDisplayKind(&cfg, (WebBootDisplayKind)v)`.
- **`void*` casts**: `malloc`/`calloc` results must be cast to the target
  pointer type.
- **Designated initializers** out of order are an error in C++ (must match
  declaration order).
- **Unused variables** warn (e.g. an `iso` you didn't end up needing) — drop
  them.
- **C headers without `extern "C"` guards** (e.g. `ada_c.h`): wrap the
  `#include` in `extern "C" { ... }` so the C++ TU references the C-linkage
  symbols the library actually exports (else: undefined reference to mangled
  names at link).

### Throwing errors

V8 has no return-value sentinel like `JS_EXCEPTION`. To throw: schedule the
exception and **return immediately**:

```cpp
iso->ThrowException(Exception::TypeError(nx_str(iso, "bad arg")));
return;   // GetReturnValue is ignored once an exception is pending
```

Helper: `nx_throw(iso, "msg")` / `nx_throw_fmt(iso, fmt, ...)` in `error.cc`.

When calling into JS (`fn->Call`, `obj->Get` on a proxy, etc.) wrap in a
`TryCatch` if you need to observe/forward the error; otherwise let it propagate
(an empty `MaybeLocal` means an exception is already pending — return without
clearing it).

### ArrayBuffers (replaces `JS_NewArrayBuffer` + free callback)

```cpp
// Take ownership of `ptr` (size n), freed with free() when the AB is GC'd:
std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
    ptr, n,
    [](void *data, size_t, void *) { free(data); }, nullptr);
Local<ArrayBuffer> ab = ArrayBuffer::New(iso, std::move(bs));
```

To read an incoming buffer (don't free it): get its `BackingStore` via
`ab->GetBackingStore()` → `->Data()` / `->ByteLength()`. For a `TypedArray`
arg, use `->Buffer()`, `->ByteOffset()`, `->ByteLength()`.

---

## 9. `nx_context_t` and reaching global state

`nx_context_t` is stored on the isolate: `iso->SetData(0, nx_ctx)` at startup;
read via `nx_ctx(iso)` everywhere (replaces `JS_GetContextOpaque`/
`JS_GetRuntimeOpaque`). Its retained `JSValue` handlers become
`v8::Global<v8::Function>`. It also gains `uv_loop_t *loop`.

---

## 10. Things being deleted (do not port)

- `wasm.cc` / `wasm.h` / `wasm.ts` — V8 ships native `WebAssembly` (JIT build
  installs it on every context automatically). Remove the wasm3 bridge and
  `-lm3`.
- `poll.c`, `thpool.c` — replaced by the libuv loop + threadpool.
- `runtime.c` (qjsc bytecode blob) — `runtime.js` is embedded as source and
  evaluated at boot.
- `memory.c` QuickJS specifics (`JS_ComputeMemoryUsage`) — re-expose via
  `Isolate::GetHeapStatistics`.

---

## 11. Checklist before marking a module "ported"

- [ ] `.c` → `.cc`, header declares only `nx_init_foo(Isolate*, Local<Object>)`.
- [ ] No exceptions/RTTI; compiles under `-fno-exceptions -fno-rtti`.
- [ ] All V8 API use is on the loop thread; `uv_work_t` work cb is V8-free.
- [ ] `HandleScope` opened in every callback that creates handles.
- [ ] Native handles via internal field + `SetAlignedPointerInInternalField`.
- [ ] Finalizer frees native state and is idempotent with any explicit close.
- [ ] Errors thrown via `ThrowException` + immediate `return`.
- [ ] Promises via `nx_queue_async`; resolver held as `Global`.
- [ ] `$` functions registered with `NX_SET_FUNC`; prototype decorated with
      `NX_DEF_GET`/`NX_DEF_GETSET`/`NX_DEF_FUNC`.
- [ ] TS `$.ts` signature unchanged (or updated in lockstep if it had to change).
