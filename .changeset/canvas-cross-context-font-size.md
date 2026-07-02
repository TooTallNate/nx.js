---
"@nx.js/runtime": patch
---

fix: cross-context canvas font size leakage — re-pin the shared FT_Face char_size at the start of `fillText()`, `strokeText()`, and `measureText()`
