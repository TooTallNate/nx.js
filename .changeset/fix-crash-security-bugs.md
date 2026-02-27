---
'nx.js': patch
---

Fix high-severity crash/security bugs in C runtime:
- Replace VLA with heap allocation in `decode_png` to prevent stack overflow with malicious PNGs (#229)
- Add NULL check for `JS_ToCString` return in `js_print` and `js_print_err` (#228)
- Free loading image framebuffer after rendering to prevent leak in console-mode apps (#227)
