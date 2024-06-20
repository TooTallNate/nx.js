---
"nxjs-runtime": patch
---

Implement `navigator.getGamepads()`
 - Removes non-standard `buttondown` and `buttonup` events
 - Use `preventDefault()` on "beforeunload" event to prevent exiting
