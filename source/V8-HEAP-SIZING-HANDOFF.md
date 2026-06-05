# Handoff: V8 heap sizing in application mode (`source/main.cc`)

Context for the nx.js agent. Covers what changed in `switch-v8` and what (if
anything) to change in nx.js's V8 instantiation. Source of truth for the
upstream fixes: `pacman-packages/switch/v8/HEAP-COMMIT-INVESTIGATION.md`.

## TL;DR

- **Required:** none. The recent `switch-v8` fixes (full-JIT GC W^X Data Aborts)
  are entirely inside the monolith. Just rebuild/relink nx.js against
  **`switch-v8 >= 15.0.243-9`** and the full-JIT crashes are gone.
- **Recommended bug fix:** nx.js currently sizes V8's max heap from
  `mem_free = TotalMemorySize - UsedMemorySize`, which is only a few MiB in
  application mode (the grant is reported as "used"). That underflows the heap to
  the **32 MiB floor in application mode** — i.e. a ~3 GiB device runs a 32 MiB
  JS heap and `FatalOOM`s on memory-heavy JS. Fix the heap budget to use the real
  ceiling. Details below.

## Background: what was fixed upstream (no nx.js change needed)

On Horizon, full-JIT executable memory is a libnx jit_* region with a read-only
**rx** alias (writes go through the **rw** alias = rx + delta). Two V8 GC paths
wrote *data* into the start of a CODE page via the rx alias and Data-Aborted —
**only under full JIT**, which is why it showed up "even in application mode":

- `MutablePage::SetOldGenerationPageFlags` (marking barrier) — fixed by switch-v8
  patch 0008 (redirect the in-page `MemoryChunk` flag store to the rw alias).
- `Sweeper::ZeroOrDiscardUnusedMemory` (GC sweeper `memset`) — fixed by switch-v8
  patch 0009 (skip the cosmetic zeroing for executable pages).

Verified on hardware: full JIT in application mode went from a Data Abort at
32 MiB → graceful `RangeError` at ~3 GiB. **Action: install/link `switch-v8
15.0.243-9`** (an older `-7`/`-8` still has these bugs).

## The nx.js bug to fix: application-mode heap underflow

In `source/main.cc`, the JIT gate correctly keys on `mem_total`
(`tight_memory = mem_total <= 1 GiB`, ~line 898), but the **heap budget** still
uses `mem_free`:

```c
// ~line 973
u64 reserve   = (can_jit ? 180ull : 48ull) * 1024 * 1024;
u64 max_heap  = mem_free > reserve ? mem_free - reserve : 0;   // <-- mem_free is wrong here
if (max_heap < 32ull * 1024 * 1024)  max_heap = 32ull * 1024 * 1024;
if (max_heap > 192ull * 1024 * 1024) max_heap = 192ull * 1024 * 1024;
create_params.constraints.ConfigureDefaultsFromHeapSize(8 MiB, max_heap);
```

The code's own comment (~line 886) already notes `UsedMemorySize` reports the
whole grant as used in application mode, so `mem_free ≈ 3 MiB` there →
`max_heap` underflows to the 32 MiB floor. That is the original
`HEAP-COMMIT-INVESTIGATION` "Symptom A" (`FatalOOM` on a big `Array.prototype.join`).

### Recommended fix (as implemented)

Budget the heap **per memory regime** — `mem_free` is only meaningful in
applet/tight-memory mode, so it must NOT be used in application mode (where it
reads as only a few MiB even with a multi-GiB grant). This is what landed in
`source/main.cc`:

```c
// Declare alongside the other horizon_mman_* externs (near line 84):
extern "C" size_t horizon_mman_data_arena_size(void);

// ... in the heap-sizing block, replace the mem_free math:
{
    u64 arena   = (u64)horizon_mman_data_arena_size();
    // Headroom for the JIT code arena (~128 MiB real when can_jit), GPU/Mesa or
    // Cairo, libuv, and native allocs — all share this memory.
    u64 reserve = (can_jit ? 180ull : 48ull) * 1024 * 1024;
    u64 ceiling;
    if (tight_memory) {
        // Applet (~380 MiB grant): the ~1 GiB arena is pure address space the
        // device can't back, so bound by REAL free physical RAM. Here
        // mem_free (= total - used) IS meaningful (~137 MiB).
        ceiling = mem_free;
    } else {
        // Application (multi-GiB grant): physical RAM is plentiful but mem_free
        // is unreliable — the whole grant is reported as "used" at startup, so
        // mem_free reads as only a few MiB. Size from the mman's reserved arena
        // instead (fallback 192 MiB of address space if the reservation failed,
        // which after `reserve` lands on the 32 MiB floor — conservative).
        ceiling = arena ? arena : 192ull * 1024 * 1024;
    }
    u64 max_heap = ceiling > reserve ? ceiling - reserve : 0;

    if (max_heap < 32ull  * 1024 * 1024) max_heap = 32ull  * 1024 * 1024;
    if (max_heap > 512ull * 1024 * 1024) max_heap = 512ull * 1024 * 1024; // was 192
    create_params.constraints.ConfigureDefaultsFromHeapSize(8ull << 20, max_heap);
}
```

Leave the `set_code_range_size_in_bytes` block (64 MiB for JIT / 0 for jitless)
exactly as-is — that is correct and load-bearing.

Notes:
- Do NOT use `min(arena, mem_free)` unconditionally: in application mode
  `mem_free ≈ a few MiB`, so the `min` would collapse to it and reintroduce the
  32 MiB under-sizing bug. `mem_free` is only a valid ceiling in applet mode.
- Raising the clamp from 192 → 512 MiB lets application-mode apps actually use
  the available heap. The V8 object heap lives in the (≈1 GiB) mman DATA arena,
  so 512 MiB is safe headroom-wise.
- Verified on device: application mode `max_heap=512 MiB`; applet mode
  `max_heap=89 MiB` (sized from ~137 MiB free RAM). Both stable.

## Important: ArrayBuffer / TypedArray memory is SEPARATE from the V8 heap

`ArrayBuffer::Allocator::NewDefaultAllocator()` (the allocator nx.js passes in
`create_params`, ~line 957) backs `ArrayBuffer`/`TypedArray` data with
`malloc`/`calloc` from the **newlib heap**, NOT the V8 object heap and NOT the
mman DATA arena. Consequences:

- `max_heap` does **not** bound TypedArray memory; large `Uint8Array`/`Blob`/
  buffer workloads are bounded by physical RAM directly (the on-device test
  reached ~3 GiB of `Uint8Array` while the V8 object heap used only ~32 MiB).
- That memory now degrades gracefully (catchable `RangeError`) at the wall rather
  than crashing, given switch-v8 -9 + the -7 ENOMEM change. No nx.js change
  needed for it — just don't size it via `max_heap`.

## Verifying on device

There is a standalone regression app at
`pacman-packages/examples/heap-stress` (V8-only, no Skia/libuv). It sizes the
heap from `horizon_mman_data_arena_size()`, allocates `Uint8Array`s to the wall,
and does a large `Array.join`. Expected: prints `SURVIVED (no Data Abort)` and a
caught `RangeError`, in both applet (jitless) and application (full JIT) mode.
Use it as the reference for the sizing pattern above.

For nx.js itself: after the change, confirm in application mode that
`[v8] ... mode=jit` is logged, a multi-hundred-MiB workload runs, and a big
`Array.prototype.join` (the NSP-builder shape) completes instead of `FatalOOM`.
