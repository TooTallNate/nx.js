---
"@nx.js/nro": minor
"@nx.js/nsp": minor
---

feat: the slim bootstrap launcher now downloads a compatible nx.js runtime automatically when none is installed. When a slim app (NRO or NSP) can't find a shared runtime satisfying its `[runtime] version` requirement under `sdmc:/nx.js/`, the launcher queries the nx.js GitHub releases over HTTPS (via the Switch `ssl` service), picks the highest release that satisfies the requirement, downloads its `nxjs.nro` with an on-screen progress indicator, saves it as `sdmc:/nx.js/nxjs-v<version>.nro`, and continues launching — so a freshly-installed slim app "just works" on a network-connected console without manually installing the runtime first. If the download can't proceed (no network, no satisfying release), it falls back to the existing manual-install instructions screen.
