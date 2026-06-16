---
"@nx.js/runtime": patch
---

fix: run emergency native teardown before V8 fatal/OOM exits so applet-mode homebrew does not leave hbmenu or bsdsocket corrupted
