# @nx.js/inspect

## 0.0.2-beta.0

### Patch Changes

- fix: `inspect()` no longer prints a repeated (non-circular) reference as `[Circular *n]`. Only the objects on the current path count as cycles now, so `inspect([a, a])` prints `a` twice like Node's `util.inspect`. Invalid `Date` objects print `Invalid Date` instead of throwing `RangeError: Invalid time value`. ([#429](https://github.com/TooTallNate/nx.js/pull/429))

## 0.0.1

### Patch Changes

- Add `@nx.js/inspect` package ([#147](https://github.com/TooTallNate/nx.js/pull/147))
