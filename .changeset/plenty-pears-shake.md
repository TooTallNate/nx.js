---
"@nx.js/runtime": patch
---

fix: default to the jitless interpreter in the applet regime again (`[v8] jit = auto`). The JIT's 64 MiB code-range minimum is dual-mapped by libnx jitCreate to ~128 MiB of real memory — a third of the applet grant — leaving so little slack that the console's canvas terminal (and other multi-MiB allocations) degrade or fail. Jitless keeps the full canvas-terminal experience reliably working in applet mode; apps that want JIT performance there can opt in with `[v8] jit = on`. Application mode is unchanged (full JIT).
