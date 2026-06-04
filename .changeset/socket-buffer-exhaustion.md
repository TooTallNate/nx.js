---
"@nx.js/runtime": patch
---

fix: right-size the socket transfer-memory pool and plug fd leaks on failed connects, `Socket.close()`, and failed WebSocket handshakes so concurrent `fetch()`/redirect/WebSocket workloads no longer fail with "No buffer space available" (ENOBUFS).
