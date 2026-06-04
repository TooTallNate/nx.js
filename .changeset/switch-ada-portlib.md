---
"@nx.js/runtime": patch
---

fix: link the [ada](https://github.com/ada-url/ada) URL parser from the `switch-ada` portlib instead of vendoring its amalgamation, upgrading it from 2.9.2 to **3.4.4**. `Switch.version.ada` is now reported dynamically from ada's `ADA_VERSION` (was hard-coded). The upgrade fixes a URL parsing bug with surrogate / noncharacter code points in the path and query (a previously-skipped WHATWG URL test now passes).
