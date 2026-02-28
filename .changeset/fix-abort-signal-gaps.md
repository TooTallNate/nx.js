---
'@nx.js/runtime': patch
---

Fix AbortController/AbortSignal spec conformance: default abort reason is now a `DOMException` with name `"AbortError"`, and implement `AbortSignal.abort()`, `AbortSignal.timeout()`, and `AbortSignal.any()` static methods.
