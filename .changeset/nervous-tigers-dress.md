---
"@nx.js/http": patch
---

Refactor HTTP server implementation
  - Support Keep-Alive by default
  - Support `Content-Length` and `Transfer-Encoding: chunked` bodies
  - Add tests using `vitest`
