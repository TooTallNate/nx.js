---
"@nx.js/runtime": patch
---

fix: clone AES contexts per-operation in async crypto workers to prevent thread-unsafe sharing
