---
'@nx.js/runtime': patch
---

fix: load system CA certificates individually to work around libnx `sslGetCertificates()` bounds-check bug with `SslCaCertificateId_All`
