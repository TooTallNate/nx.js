---
'@nx.js/runtime': patch
---

Fix TextDecoder spec compliance: correct 3-byte UTF-8 bitmask (0x0f instead of 0x1f), implement `fatal` mode to throw TypeError on invalid sequences, implement `ignoreBOM` option to strip/preserve BOM, and add constructor options support.
