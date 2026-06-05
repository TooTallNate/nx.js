---
"@nx.js/runtime": minor
---

feat: native ES module `import` resolution — the runtime now resolves static `import` and filesystem `await import()` against `romfs:`/`sdmc:`/`nxjs:` (relative and absolute-URL specifiers; bare specifiers throw), with a module cache for referential stability/cycles and correct per-module `import.meta.url`/`import.meta.main`. Top-level await is supported. Enables unbundled multi-file apps and app-level lazy loading (JSON/asset and remote `http(s):`/`data:` imports are not yet supported).
