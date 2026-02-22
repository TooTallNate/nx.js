---
"@nx.js/runtime": patch
---

refactor(web): rename `offline:` URL protocol to `htmldoc:` and abstract away internal NCA structure. Users now reference files directly (e.g. `htmldoc:/index.html`) — the `.htdocs/` prefix required by the Switch offline applet is prepended automatically.
