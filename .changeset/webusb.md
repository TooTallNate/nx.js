---
"@nx.js/runtime": minor
---

feat: implement initial WebUSB support (`navigator.usb`) backed by libnx `usb:hs`

Includes USB device discovery by descriptor filters, opening/closing devices,
claiming interfaces, bulk `transferIn()` / `transferOut()`, control
`controlTransferIn()`, and device reset. Adds a `webusb-rcm` example that sends
fusee/hekate-style RCM payloads to a v1 Nintendo Switch in RCM mode.
