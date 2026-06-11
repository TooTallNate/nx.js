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
- Applet+JIT memory reserve raised 64 → 96 MiB (V8 max heap ~41 MiB) so
  native-heavy workloads like streaming zstd decompression (8 MiB window)
  complete instead of exhausting the ~168 MiB applet native heap.
- V8 fatal errors / OOMs now log diagnostics to `nxjs-debug.log` and exit
  cleanly back to hbmenu instead of aborting (which poisoned the system in
  applet mode).
- `Switch.memoryUsage()` gains `externalMemory` and `nativeHeapTotal` /
  `nativeHeapArena` / `nativeHeapUsed` / `nativeHeapFree` (newlib mallinfo)
  fields for diagnosing native memory pressure.
