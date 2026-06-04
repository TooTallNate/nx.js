---
"@nx.js/runtime": patch
---

fix: build the published runtime against switch-v8 15.0.243-6, which raises `String::kMaxLength` back to the full limit so JS bundles larger than 1 MiB can be compiled.
