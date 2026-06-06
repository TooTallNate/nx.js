---
"@nx.js/runtime": minor
---

feat: optional `nxjs.ini` config file (read next to the entrypoint, before V8 init) lets an app override V8 JIT mode + flags (`[v8] jit/flags`), the V8 heap limit (`[memory] heap_limit`, clamped to what fits), the renderer (`[renderer] mode = auto|cpu|gpu`), and the libnx socket config (`[socket]` fields incl. `service_type`); effective values are exposed on `$.config`, and any value that can't be honored is logged to `nxjs-debug.log` with the reason.
