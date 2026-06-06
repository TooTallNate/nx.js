---
"@nx.js/runtime": patch
---

chore: clean up all C++ compile warnings — consume the `[[nodiscard]]` results of V8 `Maybe::To()`/`MaybeLocal::ToLocal()` in audio/crypto/service (defaulting on failure), replace a truncating `strncpy` with `snprintf`, value-initialize the `sk_sp`-containing font-face struct with placement-new instead of `memset`, and suppress unavoidable third-party header warnings (Skia's `clang::reinitializes` attribute, V8's `GetIsolate()` strict-aliasing type-pun). The device build is now warning-free.
