---
"@nx.js/runtime": patch
---

fix: `Video.play()` no longer rejects with `InvalidStateError` when called before `loadedmetadata` — playback is queued and the returned promise resolves once it actually begins, matching `HTMLMediaElement.play()`. A pending `play()` is rejected with `AbortError` by `pause()` or a superseding load, and with `NotSupportedError` on load failure
