---
"@nx.js/runtime": minor
---

feat: configurable libuv worker thread pool via `[threadpool]` in `nxjs.ini`, with applet-safe defaults

The libuv worker pool (which services every async native op — fs, crypto,
compression, image decode, dns) previously initialized lazily with upstream
defaults of 4 threads × 8 MiB stacks. In applet mode that 32 MiB commit could
not be satisfied next to the JIT code arena, so the **first async operation
hard-aborted the process** (skipping the applet exit handshake and
destabilizing the system until reboot).

- New `[threadpool]` section in `nxjs.ini`: `size` (worker count) and
  `stack_size` (per-worker stack, 256 KiB floor / 32 MiB cap). Defaults:
  2 workers × 1 MiB in applet mode, 4 × 1 MiB in application mode. Effective
  values are exposed on `$.config.threadpool`. Requires `switch-libuv`
  ≥ 1.52.1-3 (adds the `UV_THREADPOOL_STACK_SIZE` env var).
- Native-heap back-pressure for high-rate async allocators: after each async
  op, if the native malloc heap is running low the runtime fires a V8
  `MemoryPressureNotification(kCritical)` to reclaim unreferenced external
  `ArrayBuffer` backing stores. Without it, a tight async loop (e.g. reading a
  `DecompressionStream` chunk by chunk) let external buffers pile up faster
  than V8's GC scheduled around its tiny managed heap, exhausting the
  ~30 MiB free applet native heap in seconds. Verified on-device: a full
  594 MB streaming zstd decompression that previously OOM'd at ~8 MB now runs
  to completion in applet mode.
- Async after-work callbacks no longer crash when they fire during teardown
  (e.g. `Switch.exit()` / HOME pressed mid-operation): the callback captures
  the queuing context and falls back to it (or drops the result) instead of
  entering an empty `v8::Context` (a Data Abort at 0x0 in `Context::Enter()`).
- Applet+JIT memory reserve raised 64 → 96 MiB (V8 max heap ~41 MiB) for the
  opt-in `[v8] jit = on` applet case (the default applet regime is jitless).
- V8 fatal errors / OOMs now log diagnostics to `nxjs-debug.log` and exit
  cleanly back to hbmenu instead of aborting (which poisoned the system in
  applet mode).
- `Switch.memoryUsage()` gains `externalMemory` and `nativeHeapTotal` /
  `nativeHeapArena` / `nativeHeapUsed` / `nativeHeapFree` (newlib mallinfo)
  fields for diagnosing native memory pressure.
