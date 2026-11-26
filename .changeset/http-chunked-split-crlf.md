---
"@nx.js/http": patch
---

fix: chunked request bodies no longer fail with `Expected \r\n after data chunk` when a chunk line ends exactly at the end of a read. `readUntilEol()` now always strips the trailing CRLF.
