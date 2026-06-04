---
"@nx.js/runtime": patch
---

fix: never hard-crash the console from native bindings — route data-driven failures (invalid-UTF-8 names, hostile getters, allocation failures) into catchable JS errors instead of aborting the process.
