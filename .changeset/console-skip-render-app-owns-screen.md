---
"@nx.js/runtime": patch
---

perf: stop re-rendering the on-screen console every frame once an app owns the screen. After `Screen.getContext('2d')` the per-frame `presentConsole()` still called `Terminal.render()` each frame; that is a no-op while the terminal is clean, but any `console.*` output marks it dirty, so the next frame re-rasterizes the entire (now invisible) console text canvas. An app that logs every frame (e.g. per-tick telemetry) then pays that re-rasterize cost every frame for pixels nobody sees — a hard-to-spot per-frame hitch on constrained targets. Apps that composite `console.canvas` themselves still get it rendered on demand via the lazy `canvas` getter.
