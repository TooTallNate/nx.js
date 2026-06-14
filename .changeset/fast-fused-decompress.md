---
"@nx.js/runtime": minor
---

perf: fast native fused read+decompress for `file.stream().pipeThrough(DecompressionStream)`

Streaming a file through a `DecompressionStream` —
`Switch.file(path).stream().pipeThrough(new DecompressionStream('zstd'))`, the
pattern an NSZ/NSP installer uses — was dramatically slow (~0.6 MB/s on-device)
and, in the memory-constrained applet regime, could exhaust the native heap and
crash. The cost was per-chunk overhead: every chunk meant a separate `$.fread`
thread-pool dispatch, a separate `$.decompressWrite` dispatch, the
web-streams-polyfill pipe's ~10 promises, and several fresh `ArrayBuffer`s,
plus the decoder's realloc-grow spikes.

- New native **fused read+decompress** op: opens the file once and, per pull,
  reads a chunk AND decompresses it into a fixed reused buffer in the *same*
  thread-pool dispatch — no polyfill pipe, no per-chunk read/decompress split,
  no realloc spikes. Supports all `DecompressionStream` formats (zstd, gzip,
  deflate, deflate-raw).
- The `file.stream().pipeThrough(new DecompressionStream(...))` pattern detects
  this combination and routes to the fused path **transparently** — no app
  code change. Any other source/transform pair keeps the existing behavior.
- Per-pull output size and `FsFile.stream()`'s default chunk size are gated by
  memory regime (larger in the application regime, conservative in applet) so
  the win is safe in both.

Measured on-device with a 595 MB NSZ decompression (`file → zstd → AES-CTR →
sink`):
- Application regime: **~1030s → ~21s** (~28 MB/s, ~49×) — at parity with the
  native DBI installer.
- Applet regime: completes in **~120s** (was ~1030s after the prior OOM fix),
  staying within the native-heap budget.
- Pure decompression (no AES, e.g. gzip `fetch().body`): **~10 MB/s** in applet
  (~16× the previous 64 KiB-chunk path).
