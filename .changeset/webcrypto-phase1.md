---
"@nx.js/runtime": patch
---

Expand WebCrypto API with `generateKey()`, `exportKey('raw')`, HMAC `sign()`/`verify()`, and fix `CryptoKey.type` fall-through bug

- **Fix**: `CryptoKey.type` getter now returns the correct type instead of always returning `"secret"` (missing `break` statements in switch)
- **Feature**: `SubtleCrypto.generateKey()` for AES-CBC, AES-CTR, AES-GCM, and HMAC algorithms
- **Feature**: `SubtleCrypto.exportKey('raw')` to export raw key material from extractable keys
- **Feature**: `SubtleCrypto.sign()` and `SubtleCrypto.verify()` for HMAC (SHA-1, SHA-256, SHA-384, SHA-512)
- **Feature**: `SubtleCrypto.importKey('raw')` now supports HMAC keys
- **Internal**: Raw key material is now stored in `CryptoKey` for export support
