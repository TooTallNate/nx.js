---
"@nx.js/runtime": patch
---

fix: install WebGL2 `GL_CONSTANTS` with a single bulk `Object.defineProperties()` call per target instead of ~740 sequential `Object.defineProperty()` calls at module scope
