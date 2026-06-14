---
"@nx.js/runtime": patch
---

fix: prevent `AmbientLightSensor.start()` from leaking timers when called multiple times. `start()` is now a no-op while already active, and `stop()` resets the timeout handle so `activated` correctly returns `false` afterward.
