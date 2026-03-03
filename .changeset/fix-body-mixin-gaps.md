---
'@nx.js/runtime': patch
---

Fix Body mixin spec conformance issues:

- Throw `TypeError` when body is consumed more than once (`bodyUsed` check in all consumption methods)
- Make `bodyUsed` reflect stream disturbance (returns `true` when body stream is locked)
- Fix `bufferSourceToArrayBuffer` slice offset bug for TypedArrays with non-zero `byteOffset`
- Fix FormData serialization: multipart boundary delimiters now correctly use `--` prefix per RFC 2046
- Fix FormData parsing: search for `--boundary` delimiter instead of raw boundary string
