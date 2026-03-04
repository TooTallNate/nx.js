---
'@nx.js/runtime': patch
---

Fix `Request` class bugs: `credentials` from `init` now properly overrides input Request's value, `referrer` is copied from input Request, avoid creating unnecessary `AbortController` per Request by using a shared frozen signal, and `Request.clone()` now properly tee()'s the body stream.
