---
"@nx.js/runtime": patch
---

fix: `createInternal()` falsely rejects falsy values like `0n`, and `KeyboardEvent` now supports standard `KeyboardEventInit` properties
