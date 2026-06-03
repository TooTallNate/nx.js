---
"@nx.js/runtime": patch
---

fix: import the `DOMException` polyfill instead of referencing the global. Seven modules (crypto, websocket, fetch, audio, performance, canvas-gradient, abort-controller) used the global `DOMException`, which esbuild renamed to `DOMException2` in the bundle (registering the class under the wrong name). Import it from `./dom-exception` so it resolves correctly.
