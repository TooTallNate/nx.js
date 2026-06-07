---
"@nx.js/runtime": patch
---

fix: forcing full V8 JIT in applet mode (`[v8] jit = on` in `nxjs.ini`) no longer crashes with a "Fatal OOM: Zone". The JIT memory reserve was a fixed 180 MiB — an application-mode figure dominated by the GPU/Mesa stack — which underflowed the applet regime's ~137 MiB of free RAM and collapsed the V8 heap to the 32 MiB floor, far too small for the JIT/WebAssembly compiler. The reserve is now regime-aware: applet mode (no GPU; raster) reserves only the 64 MiB JIT code range plus headroom, leaving a usable (~70+ MiB) heap, so full JIT — and therefore WebAssembly, which requires a code-generation backend — works in applet mode.
