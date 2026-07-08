---
"@nx.js/runtime": patch
---

feat: `Image`, `Audio`, and `Video` now resolve `globalThis.fetch` at call time, so embedder-installed `fetch` wrappers (e.g. custom URL schemes) are honored for `src` loads
