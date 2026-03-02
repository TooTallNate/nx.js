---
'@nx.js/runtime': patch
---

Fix `URLSearchParams.get()` to return `null` for missing keys instead of `""`
