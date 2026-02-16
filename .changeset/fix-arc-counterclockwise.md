---
"@nx.js/runtime": patch
---

Fix `arc()` counterclockwise parameter — was reading out-of-bounds `argv[6]` instead of `argv[5]`, and now properly defaults to `false` when omitted
