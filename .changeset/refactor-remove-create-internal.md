---
"@nx.js/runtime": patch
---

refactor: migrate 9 classes from `createInternal` WeakMap pattern to native `#private` class fields; upgrade esbuild from 0.17 to 0.27 so the bundle preserves native `#field` syntax instead of transpiling to WeakMap helpers
