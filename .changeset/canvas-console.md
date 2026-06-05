---
"@nx.js/runtime": minor
---

feat: canvas-backed `console` — `console.log`/`warn`/`error` now render to the screen via a headless xterm.js terminal drawn with Canvas2D in the bundled Geist Mono font (ANSI colors, UTF-8, scrollback with `console.scrollUp`/`scrollDown` + touch-drag, plus `console.canvas` and isolated `new Console()` instances), falling back to the native libnx console when no font/display is available; this also fixes the GPU-mode `console.log` crash (it no longer tears down EGL). The on-screen "Runtime initialization failed" message now shows the underlying error + stack.
