---
'@nx.js/runtime': patch
---

`Response.redirect()` now parses the URL before setting the `Location` header, normalizing it per the Fetch spec (e.g. `https://example.com` becomes `https://example.com/`)
