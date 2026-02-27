---
'@nx.js/runtime': patch
---

Replace hand-rolled base64 with mbedtls: use `mbedtls_base64_encode()`/`mbedtls_base64_decode()` for `atob`/`btoa`, and add native `$.base64urlEncode()`/`$.base64urlDecode()` to replace JS-land helpers
