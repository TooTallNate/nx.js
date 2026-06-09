---
"@nx.js/runtime": patch
---

fix: `console.canvas` now keeps updating after an app takes over the screen with `screen.getContext('2d')`. Previously, once the app owned the screen the runtime stopped rendering the console terminal, so an app compositing `console.canvas` itself (e.g. `ctx.drawImage(console.canvas, ...)`) — especially when it cached the canvas reference once — saw it frozen at the last auto-presented frame. The per-frame present now still renders the terminal (without blitting it) while the app owns the screen, and accessing `console.canvas` renders any pending output on demand, so composited console output stays live.
