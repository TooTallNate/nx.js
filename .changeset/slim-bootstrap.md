---
"@nx.js/nro": minor
---

feat: slim app packaging via `nxjs-nro --slim` — builds a tiny launcher NRO (a few hundred KB) instead of embedding the full ~40 MB runtime; at boot it chainloads a shared runtime installed at `sdmc:/nx.js/nxjs-v<version>.nro`, selected by the `[runtime] version` semver requirement in the app's `romfs/nxjs.ini` (defaulting to caret-on-major). `create-nxjs-app` now prompts to package an app as slim or fat.
