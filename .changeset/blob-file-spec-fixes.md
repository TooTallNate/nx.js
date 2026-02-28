---
'@nx.js/runtime': patch
---

Fix `Blob` constructor to lowercase the `type` option per the W3C FileAPI spec. Fix `File.lastModified` to use `Date.now()` as default instead of unnecessary `new Date()` conversion.
