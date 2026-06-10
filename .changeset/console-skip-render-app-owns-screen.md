---
"@nx.js/runtime": patch
---

perf: stop re-rendering the on-screen console every frame once an app owns the screen, unless the app has read `console.canvas`. After `Screen.getContext('2d')` the per-frame `presentConsole()` re-rasterized the entire (now invisible) console text canvas any frame that `console.*` output had marked it dirty — an app logging every frame (e.g. per-tick telemetry) paid that cost every frame for pixels nobody sees. Apps that composite the console themselves are unaffected: reading `console.canvas` (even once, cached) marks the console as observed, and the per-frame render then continues exactly as before so a cached canvas reference stays live.
