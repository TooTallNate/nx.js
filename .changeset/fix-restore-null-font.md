---
"@nx.js/runtime": patch
---

Fix `restore()` crash when no font face is set — added null check before calling `cairo_set_font_face`
