---
"@nx.js/runtime": patch
---

fix: harden display bring-up and large allocations against the applet memory cliff. The applet regime (~380 MiB grant) leaves little slack once V8 + the runtime are up, yet the raster framebuffer needs ~11.2 MiB at init time. This pre-funds the display path (a process-lifetime nvdrv reference taken at boot to avoid an nvservices hang in the nvdrv close→reopen, plus a ~12 MiB "display parachute" released right before `framebufferCreate`), checks the `framebufferCreate`/`framebufferMakeLinear` results (black-screen fallback instead of a `memcpy(NULL)` crash), and switches `getSystemFont`/`getImageData` to checked allocations + `ArrayBuffer::NewBackingStore` so an out-of-memory condition throws a catchable error instead of fatally aborting the process (the console then gracefully falls back to the libnx PrintConsole).
