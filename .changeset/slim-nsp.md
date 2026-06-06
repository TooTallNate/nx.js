---
"@nx.js/runtime": minor
"@nx.js/nsp": major
"@nx.js/nro": patch
---

feat: slim NSP packaging. `nxjs-nsp` now builds a **slim** NSP by default — its exefs `main` is a tiny forwarder (a patched nx-hbloader) that chainloads the shared runtime NRO from `sdmc:/nx.js/` (selected by the `[runtime] version` requirement in the app's `romfs/nxjs.ini`, default caret-on-major) and mounts the installed title's own RomFS (the app) into it — instead of embedding the full ~21 MB runtime NSO per title. Pass `--fat` (or `NXJS_FAT=1`) for a self-contained NSP. The runtime gained an `argv[1] == "nsp:"` entrypoint that mounts the app via `romfsMountFromCurrentProcess`. The `bootstrap/` launcher sources were reorganized into shared logic + `launcher-nro/` and `launcher-nsp/`; `@nx.js/nro`'s slim base path moved accordingly. `create-nxjs-app` flags both the `nro` and `nsp` scripts for the Fat packaging choice.
