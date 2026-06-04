---
"@nx.js/runtime": patch
---

fix: fail gracefully with a clear error instead of hard-crashing (V8 `OS::Abort`) when the entrypoint source exceeds V8's maximum string length.
