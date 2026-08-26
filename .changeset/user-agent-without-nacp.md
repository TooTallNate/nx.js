---
'@nx.js/runtime': patch
---

fix: `navigator.userAgent` no longer throws when the application's NACP cannot be read. `Application.self.name` throws "No language entry found" whenever the running title cannot be resolved — the normal state under an emulator — and since `fetch()` fills the `user-agent` header in for callers who did not set one, that made every request fail before reaching the network. The app name and version now fall back to `unknown/0.0.0` while the Horizon and nx.js versions, which are still known, are still reported. `Application.self.name` itself is unchanged and still throws.
