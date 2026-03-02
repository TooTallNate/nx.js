---
'@nx.js/runtime': patch
---

Fix EventTarget/Event spec conformance gaps:
- `stopImmediatePropagation()` now works (sets flag checked by `dispatchEvent`)
- `stopPropagation()` is now a no-op instead of throwing
- `composedPath()` returns `[target]` instead of throwing
- `initEvent()` performs basic re-initialization instead of throwing
- `timeStamp` is set to `performance.now()` at event creation time
- `composed` option from `EventInit` is now respected
- `signal` option on `addEventListener` auto-removes listener when aborted
