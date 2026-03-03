---
"@nx.js/runtime": patch
---

Fix FormData `set()` to remove all subsequent duplicate entries per spec, and use `"blob"` as default filename for Blob values in `append()`/`set()` instead of the field name.
