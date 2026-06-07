---
"@nx.js/runtime": minor
---

feat: enable the V8 JIT by default in applet mode too. Previously applet mode (the low-memory regime) ran the jitless interpreter; it now runs full JIT in both regimes, which substantially speeds up JS execution in applet mode. This is safe now that the JIT code-arena headroom is regime-gated (applet reserves the 64 MiB code-range minimum, leaving room for the heap/canvas/sockets) — verified stable across the example apps on-device in applet mode. Applet mode still uses CPU raster rendering (chosen independently of JIT). Force the interpreter with `[v8] jit = off` in `nxjs.ini`.

WebAssembly, which needs extra JIT code-arena headroom that applet mode can't afford by default, now fails fast with an explicit, actionable error (pointing at `[v8] wasm = on` / `code_headroom_mb`) instead of a cryptic `CompileError` or crash when used without that headroom (or when JIT is disabled). It works out of the box in application mode.
