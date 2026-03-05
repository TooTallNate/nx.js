---
'@nx.js/http': patch
---

Fix HTTP server error handling and header bugs:
- Add try/catch around request handler to return 500 on errors and always close socket
- Await `w.write()` in `writeResponse` to ensure headers are flushed before body
- Use `headers.append()` instead of `headers.set()` in `toHeaders()` to preserve duplicate headers
- Only set default `content-type: text/plain` when response has a body
