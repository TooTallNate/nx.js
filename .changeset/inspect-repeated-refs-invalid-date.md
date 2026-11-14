---
"@nx.js/inspect": patch
---

fix: `inspect()` no longer prints a repeated (non-circular) reference as `[Circular *n]`. Only the objects on the current path count as cycles now, so `inspect([a, a])` prints `a` twice like Node's `util.inspect`. Invalid `Date` objects print `Invalid Date` instead of throwing `RangeError: Invalid time value`.
