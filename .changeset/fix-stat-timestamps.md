---
"@nx.js/runtime": patch
---

Fix `statToObject` atime/ctime timestamps — was using `tv_nsec` (nanoseconds) instead of `tv_sec` (seconds)
