---
'nxjs-runtime': patch
---

Add threadpool, with new asynchronous functions:
 * `Switch.readFile() -> Promise<ArrayBuffer>`
 * `Switch.resolveDns() -> Promise<string[]>`
