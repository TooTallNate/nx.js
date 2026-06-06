---
"@nx.js/runtime": patch
"@nx.js/nro": patch
"@nx.js/nsp": patch
---

fix: `Switch.Application.self` now resolves to the launched app (not the shared runtime) in slim packaging modes. It previously keyed off `$.argv[0]`, which for a slim NRO/NSP is the shared runtime NRO, so `self.name` reported `"nx.js"` instead of the app. The runtime now exposes `$.selfNroPath` (the app's `.nro` path for standalone/slim NRO apps, or `null` for installed titles so `self` resolves via the process's `ProgramId`), and the slim bootstrap launcher's NACP carries the proper author + title id so a slim app's `Application.self` (name/author/version/id) matches the fat build. Verified on-device across all four modes (fat/slim × NRO/NSP).
