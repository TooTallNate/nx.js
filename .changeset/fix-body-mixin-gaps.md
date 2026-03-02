---
'@nx.js/runtime': patch
---

Fix Body mixin: throw `TypeError` when body is consumed more than once, and fix `bufferSourceToArrayBuffer` offset bug for TypedArrays with non-zero `byteOffset`
