---
"@nx.js/nro": major
---

`nxjs-nro` now builds a **slim** NRO by default (a tiny launcher that chainloads a shared runtime installed at `sdmc:/nx.js/`), instead of a self-contained ~40 MB NRO. Pass `--fat` (or set `NXJS_FAT=1`) to embed the runtime as before. `--slim` / `NXJS_SLIM=1` are still accepted as no-ops. `create-nxjs-app` adds `--fat` to the generated `nro` script when the "Fat" packaging option is chosen.
