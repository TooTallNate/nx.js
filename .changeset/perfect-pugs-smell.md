---
'nxjs-runtime': patch
---

Overhaul filesystem operations:

 * Added `Switch.mkdirSync()`, `Switch.removeSync()`, `Switch.statSync()`
 * Read operations return `null` for `ENOENT`, instead of throwing an error
 * `Switch.remove()` and `Switch.removeSync()` work with directories, and delete recursively
 * `Switch.writeFileSync()` creates parent directories recursively as needed
