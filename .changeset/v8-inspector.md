---
'@nx.js/runtime': minor
---

feat: expose the V8 inspector over the Chrome DevTools Protocol. `Switch.inspector.start({ port })` opens a WebSocket server that `chrome://inspect`, the Node DevTools window, and editor debuggers attach to — breakpoints, stepping, call frames and scopes, `Runtime.evaluate` in the console, and the profilers. `{ wait: true }` blocks startup until a debugger attaches and breaks on the next statement, the equivalent of `--inspect-brk`. The transport is native rather than built on `Switch.listen()` because a paused isolate runs no JavaScript, so nothing written in JavaScript could receive the message that resumes it.
