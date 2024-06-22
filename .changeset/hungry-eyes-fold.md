---
"nxjs-runtime": patch
---

Implement `navigator.getGamepads()`
 - Allows for up to 8 gamepads to be individually controlled
 - Adds support for l/r analog stick positions to be utilzed
 - Removes non-standard `buttondown` and `buttonup` events
 - Use `preventDefault()` on "beforeunload" event to prevent exiting
