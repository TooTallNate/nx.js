---
"@nx.js/runtime": patch
---

Fix HID vibration memory leak — free `JS_GetPropertyStr` values in `js_hid_send_vibration_values()`
