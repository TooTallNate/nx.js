---
'nxjs-runtime': patch
---

Add `requestAnimationFrame()`, `cancelAnimationFrame()`, and global "unload" event

This is technically a breaking change since the `Switch` "frame" and "exit" events are no longer dispatched.

To migrate from the "frame" event, use a `requestAnimationFrame` loop.
To migrate from the Switch "exit" event, use a global "unload" event.
