---
"@nx.js/nro": patch
"@nx.js/nsp": patch
---

fix: slim apps now bake a `[runtime] version` requirement of `^<full version>` (e.g. `^1.0.0-beta.2`) into their `nxjs.ini` instead of `^<major>` (e.g. `^1`). The caret-on-major specifier would accept an *older* shared runtime that predates—and therefore lacks—features the app was built against; caret-on-full-version means "at least the runtime this app was packaged with, or any newer compatible release". The bootstrap launcher's semver matcher also gained a prerelease-floor guard so a caret on a prerelease (e.g. `^1.0.0-beta.2`) correctly rejects an older prerelease of the same `x.y.z` (`1.0.0-beta.1`) rather than wrongly accepting it.
