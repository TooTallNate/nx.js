---
"@nx.js/runtime": patch
---

Fix TCP connect / TLS handshake string leak — free `JS_ToCString` result on error paths
