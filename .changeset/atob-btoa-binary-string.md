---
"@nx.js/runtime": patch
---

fix: `atob()` and `btoa()` now operate on binary strings (Latin-1, one byte per character) per the WHATWG spec, instead of round-tripping through UTF-8. Previously `btoa()` UTF-8-encoded its input (so a character like `\xff` became two bytes), and `atob()` decoded the bytes as UTF-8 (mangling any byte ≥ 0x80 into U+FFFD), which broke using `atob()` to recover raw bytes — e.g. a base64 decoder built on `atob()` failed for any byte over 127. Now every byte value 0–255 round-trips through `btoa()`/`atob()`, `btoa()` throws `InvalidCharacterError` for characters above 0xFF, and both throw a DOMException-named `InvalidCharacterError` on bad input (matching Chrome). `atob()` also implements the WHATWG forgiving-base64 decode directly (strips ASCII whitespace, accepts unpadded input, rejects `length % 4 == 1`) instead of the stricter mbedtls decoder. Adds conformance tests verified against Chrome (40 assertions, identical in both engines).
