---
'@nx.js/runtime': patch
---

Implement `OffscreenCanvas` `width`/`height` setters per the HTML Canvas spec. Previously these were no-ops (empty stubs), so setting `canvas.width` or `canvas.height` after construction had no effect. Now setting either dimension invalidates the backing Cairo surface and pixel buffer, and the next drawing operation lazily recreates them at the new size with all context state reset to defaults (fill/stroke styles, line width, transform, path, save/restore stack, etc.), matching browser behavior.
