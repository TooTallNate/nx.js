---
'@nx.js/runtime': patch
---

fix: import `DOMException` instead of referencing it as a global in `crypto.ts`, `audio.ts`, and `canvas-gradient.ts`
