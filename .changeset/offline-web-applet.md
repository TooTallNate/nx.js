---
"@nx.js/runtime": patch
---

feat: add offline mode for `Switch.WebApplet` using HtmlDocument NCA. Load HTML directly from the app's bundled content with `offline:` URLs — no network required. Supports `window.nx` bidirectional messaging via WebSession.
