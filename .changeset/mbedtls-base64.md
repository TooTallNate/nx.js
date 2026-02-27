---
'nxjs-runtime': patch
---

Replace hand-rolled base64 implementation in `atob`/`btoa` with `mbedtls_base64_encode()`/`mbedtls_base64_decode()`
