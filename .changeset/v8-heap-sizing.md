---
"@nx.js/runtime": patch
---

fix: size the V8 heap per memory regime — in application mode from the real committable arena (up to 512 MiB) instead of the misleading free-memory figure, and in applet mode from actual free RAM — so memory-heavy JS no longer fatally OOMs in application mode while applet mode stays correctly bounded.
