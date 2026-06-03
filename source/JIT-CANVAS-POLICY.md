# JIT vs. canvas backend: the correct memory policy for nx.js

Recommendation for how nx.js should choose V8's JIT mode (full JIT vs. jitless)
in relation to the canvas rendering backend (CPU/Cairo vs. GPU/Skia-GL), on the
two Switch memory regimes. Based on the on-hardware "trifecta" experiments
(V8 + Skia + libuv); see `MIGRATION-NOTES.md` for the raw findings.

## The hard constraint (measured on hardware)

The Switch runs homebrew in two memory regimes:

- **application mode** (NSP install, or hbmenu via hold-R title-redirect): ~3 GiB
  free.
- **applet mode** (NRO from Album/hbmenu): ~137 MiB free (≈381 total − ≈243 used).

V8 full-JIT needs a code range; libnx `jitCreate` **dual-maps** it (rx + rw), so
the 64 MiB minimum code range costs **~128 MiB of real memory**. The GPU path
(Skia Ganesh GL → Mesa/nouveau) needs a chunk of memory for Mesa's GLSL shader
compiler on the first draw.

In **applet mode**, 128 MiB (JIT) + Mesa does not fit in ~137 MiB → the first
GPU shader compile crashes (NULL deref in `_mesa_glsl_builtin_functions_init`).
This is a hard wall: it can't be tuned away (V8's code-range floor is 64 MiB, and
the V8 `horizon_mman_set_code_budget()` knob confirmed shrinking below that isn't
possible).

Result matrix (each cell verified at 60 fps unless noted):

| canvas | applet (~137 MiB) | application (~3 GiB) |
|---|---|---|
| **CPU** (Cairo / Skia-raster) | ✅ **full JIT** | ✅ full JIT |
| **GPU** (Skia-GL) | ⚠️ **jitless only** (full JIT ❌ crashes) | ✅ full JIT |

**Key insight: the conflict is GPU-only.** A CPU canvas never touches Mesa, so it
runs full JIT at 60 fps in *both* regimes. Only a GPU canvas in applet mode
forces the either/or.

## Recommended policy

Gate on **(canvas backend) first, then memory** — not on free memory alone:

```
if (canvas backend is CPU)            -> full JIT      // always; no Mesa contention
else /* GPU canvas */ {
    if (free_mem comfortably fits JIT + Mesa)  -> full JIT   // application mode
    else                                       -> jitless    // applet mode
}
```

### Phase 1 (today): Cairo CPU canvas → **always full JIT**

`source/main.cc` currently gates purely on `mem_free > 300 MiB` (the `can_jit`
check around line 731). For the **CPU (Cairo)** canvas this is **too
conservative**: in applet mode `mem_free ≈ 137 MiB < 300 MiB`, so nx.js drops to
**jitless even though Cairo has zero Mesa contention** — leaving JS performance on
the table for no reason. The trifecta proved CPU + full JIT = 60 fps in applet
mode.

**Action:** while the canvas is CPU-only (no Skia-GL linked), use full JIT
unconditionally. The memory gate is solving a problem that doesn't exist yet. The
in-code comment already half-acknowledges this ("Phase 1 uses a CPU (Cairo)
canvas ... so this will normally pick full JIT in both regimes") — but the 300
MiB threshold means it actually picks *jitless* in applet mode. Either drop the
gate for Phase 1, or compile it out unless a GPU canvas is present (see below).

### Phase 2 (Skia GPU canvas): the gate becomes real

Once Skia-GL is the canvas, applet mode genuinely can't have both. Recommended
default: **GPU + jitless in applet mode.** Rationale:

- nx.js is fundamentally a Canvas/graphics runtime — smooth, headroom-rich
  rendering is the headline feature; GPU gives far more drawing headroom (complex
  scenes, compositing, image ops) than CPU raster.
- The jitless penalty is on *JS execution speed*; most nx.js workloads are
  render-bound, not compute-bound, so interpreted JS driving GPU draws is fine.
- The compute-heavy case (where JIT matters most) is exactly when an app ships as
  an **NSP** (full-memory) — and there you get GPU **and** full JIT together.

### Make it expressible (ideal)

Some apps are JS-compute-heavy and rendering-light; those would prefer
**CPU canvas + full JIT** even in applet mode. So the cleanest design is to let
the app/manifest choose its canvas backend (CPU vs GPU), and derive the JIT mode
from that + the memory regime per the policy above. Keep `jitless` strictly as
the applet+GPU fallback, never the default for CPU-canvas apps.

## Concrete edits implied for `source/main.cc`

- Compute `can_jit` as: `true` if no GPU canvas is compiled in (CPU/Cairo), OR
  free memory clears the JIT+Mesa threshold; `false` only for a GPU canvas in the
  tight (applet) regime.
- Keep everything else as-is — the heap sizing (`ConfigureDefaultsFromHeapSize`),
  the 64 MiB code-range pin for JIT, and `code_range_size = 0` for jitless are all
  correct and match the hello-v8 reference.
- (Optional, full-memory builds) call `horizon_mman_set_code_budget(0, 0)` before
  V8 init in non-WASM builds to reclaim ~64 MiB of code-arena headroom; this does
  NOT enable applet+GPU+JIT (the floor still dual-maps to 128 MiB), but it frees
  memory for textures/heap when there's room. Requires switch-v8 ≥ 15.0.243-3.

## Two more applet-mode requirements for a GPU canvas (found in the 2.0 spike)

The JIT/Mesa rule above is necessary but **not sufficient**. Bringing the nx.js
runtime (V8 + libuv + sockets) up with a GPU surface in applet mode surfaced two
further blockers, both invisible in application mode (where there's GiBs of
headroom) and both fixed in `source/main.cc`:

1. **Don't gate the main loop on `appletMainLoop()` when EGL owns the window.**
   Once EGL takes the default `NWindow` for the GPU surface, `appletMainLoop()`
   can return `false` immediately, ending the loop at frame 1 — *before* any
   clean exit, so V8's `svcMapMemory` arenas leak and the next hbmenu launch is
   corrupted. Gate the loop on `is_running` instead (set by `Switch.exit()` / `+`
   / the exit syscall); still call `appletMainLoop()` inside the loop to pump
   applet messages, but only honor its `false` return as an exit signal on the
   **legacy framebuffer (CPU) path**. (Mirrors the trifecta GPU demo, which
   exits only on `+` and never gates on `appletMainLoop()`.)

2. **Scale the socket transfer-memory reservation down in applet mode.** libnx
   `socketInitialize` reserves a transfer-memory region sized roughly
   `(tcp_tx_max + tcp_rx_max + udp_tx + udp_rx) * sb_efficiency`. nx.js's full
   config is `(4+4) MiB * 8 ≈ 64 MiB` — fine in application mode, catastrophic in
   applet's ~137 MiB: it starves Mesa/V8 and the bsdsocket sysmodule faults the
   first time libuv touches its loopback self-wake socketpair (User Break at
   `bsdsocket+0xe7064`, observed at frame 1). Use a lean config when memory is
   tight (~2 MiB: 128/256 KiB buffers, `sb_efficiency 4`) and the full config
   otherwise. Both keep `BsdServiceType_Auto` (needed for privileged ports).

Both gates key off the **same** `mem_free` probe used for the JIT decision
(`tight_memory = mem_free <= 300 MiB`), probed once before `socketInitialize`.

## TL;DR

- **CPU canvas → full JIT, always** (60 fps both regimes; the conflict is
  GPU-only). Fix the Phase-1 gate so applet mode isn't needlessly jitless.
- **GPU canvas → full JIT in application mode, jitless in applet mode.** Default
  to GPU+jitless in applet for a graphics runtime; ship NSP for GPU+full-JIT.
- Ideally let the app pick the canvas backend; derive JIT mode from that + the
  memory regime.
