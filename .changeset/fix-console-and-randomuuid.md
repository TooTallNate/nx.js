---
'@nx.js/runtime': patch
---

Add missing standard `console` methods (`assert`, `count`, `countReset`, `dir`, `dirxml`, `group`, `groupCollapsed`, `groupEnd`, `table`, `time`, `timeEnd`, `timeLog`, `clear`, `info`) and fix `crypto.randomUUID()` to only allocate 16 bytes instead of 31.
