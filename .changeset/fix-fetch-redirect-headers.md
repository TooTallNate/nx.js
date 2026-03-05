---
'@nx.js/runtime': patch
---

Fix multiple `fetch()` bugs: `redirect: 'error'` now throws TypeError, headers are forwarded on redirect (with Authorization stripped on cross-origin), redirect loop detection (max 20), robust status line parsing for multi-word status text, use `append()` for response headers to support multi-value headers like Set-Cookie, and wire AbortSignal to the underlying socket.
