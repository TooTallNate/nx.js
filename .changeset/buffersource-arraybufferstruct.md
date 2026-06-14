---
"@nx.js/runtime": patch
---

fix: accept duck-typed `BufferSource` objects (e.g. `ArrayBufferStruct`) in native APIs

`NX_GetBufferSource` — used by every native API that takes a `BufferSource`
(service IPC dispatch, crypto, compression, fs, image, …) — only handled real
`ArrayBuffer` / `ArrayBufferView` values after the V8 migration. It dropped the
QuickJS-era fallback that duck-typed any object exposing
`{ buffer, byteOffset, byteLength }`.

`@nx.js/util`'s `ArrayBufferStruct` (used pervasively by `@nx.js/ncm`,
`@nx.js/install-title`, and others to describe IPC structs) is a plain JS class
that only *implements* the `ArrayBufferView` shape — it is not a real V8
`ArrayBufferView`. So passing one silently read as a null/empty buffer, which
produced malformed service requests that the kernel rejected by closing the
session (`KernelError_ConnectionClosed`, `2001-0123`). This broke, among other
things, NCM content-storage commands (`deletePlaceHolder`, `register`, `has`,
…) and therefore title installation.

Restored the duck-typed fallback (honoring `byteOffset`/`byteLength`, clamped to
the backing buffer). Verified on-device: a full NSZ title install now completes
and the title appears on the HOME menu.
