---
"@nx.js/runtime": patch
---

Fix unsafe poll list iteration — switch to `SLIST_FOREACH_SAFE` in `nx_poll()` to handle callback removals
