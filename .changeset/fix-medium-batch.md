---
"@nx.js/runtime": patch
---

fix: batch fix six medium-severity C-level bugs

- fs: prevent unsigned underflow when readFile start > end (#239)
- audio: check JS_ToInt32 return values to avoid uninitialized voice_id (#238)
- font: add FreeType/HarfBuzz error handling for invalid font data (#237)
- image: free JPEG output buffer on decompression error (#236)
- async: document threading invariant for had_error field (#235)
- main: increase js_cwd buffer to prevent stack overflow with trailing slash (#233)
