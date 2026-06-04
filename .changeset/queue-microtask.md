---
"@nx.js/runtime": patch
---

fix: restore the global `queueMicrotask()` function (a native QuickJS builtin that was missing under V8), implemented via `Isolate::EnqueueMicrotask` with callback exceptions reported through the global `error` event.
