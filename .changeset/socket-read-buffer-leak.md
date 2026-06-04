---
"@nx.js/runtime": patch
---

fix: size each socket's read buffer to the configured `tcp_rx_buf_size` (and release it eagerly on close) so opening many sockets no longer exhausts the native ArrayBuffer pool in applet mode.
