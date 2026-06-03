# nx.js engine/renderer migration — build notes

Living record of the QuickJS→V8 + Cairo→Skia + libuv migration. See
`BINDINGS.md` for the per-module code conventions; this file tracks
build-system / toolchain / integration findings.

## Toolchain & packages

- `switch-v8` (full-JIT monolith), `switch-skia` (GL/Ganesh variant),
  `switch-libuv` (v1.52.1) are installed under
  `$PORTLIBS_PREFIX = /opt/devkitpro/portlibs/switch`.
- V8 headers are **flat** in `$PORTLIBS_PREFIX/include` (`<v8.h>`,
  `<libplatform/libplatform.h>`, `<cppgc/...>`). NOT under an `include/` prefix.
- libuv is consumed via `aarch64-none-elf-pkg-config libuv`, which force-
  includes `nx-prelude.h`, adds `-D_GNU_SOURCE -DSSIZE_MAX=...`, an `-isystem`
  shim dir, and links `libuv-horizon-port.o` + `-luv`.
- Skia headers `#include "include/core/..."` and `"modules/skshaper/include/..."`
  (source-root-relative). **Was a packaging bug** (fixed in
  `switch-skia/PKGBUILD`): it did `cp -r include/* skia/` which stripped the
  `include/` prefix, so `-I .../include/skia` could not resolve the headers. Fix:
  `cp -r include skia/include` (keep the dir), so the `-I` root has `include/`
  and `modules/` subdirs. Requires a `switch-skia` rebuild.

## Validated link recipe (Phase 0.3 smoke test — PASS)

A C++ TU using both V8 (Isolate/Context/Script/microtask checkpoint) and libuv
(`uv_loop_init`/`uv_run`) **compiles and links to a valid AArch64 PIE `.nro`
with zero undefined symbols.**

Compile flags (embedder TUs are C++, `.cc`):
```
-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
-fno-rtti -fno-exceptions -std=c++20 -O2 -D__SWITCH__ -D_DEFAULT_SOURCE
$(aarch64-none-elf-pkg-config --cflags libuv)
-I $PORTLIBS_PREFIX/include -I $DEVKITPRO/libnx/include
```
- Do **NOT** define `V8_COMPRESS_POINTERS` (presence ⇒ enabled ⇒ abort).
- `-D_DEFAULT_SOURCE` (V8) and libuv's `-D_GNU_SOURCE` coexist fine.
- libnx include dir is required (libuv's `uv/unix.h` pulls `<sys/socket.h>`).

Link (g++ driver; shared Itanium ABI + devkitA64 libstdc++):
```
-specs=$DEVKITPRO/libnx/switch.specs
-march=... -mtp=soft -fPIE
<objs>
-L<patched-v8-dir> -L$PORTLIBS_PREFIX/lib
-Wl,--start-group -lv8_monolith -labsl -lchrome_zlib -lcompression_utils_portable -Wl,--end-group
$(aarch64-none-elf-pkg-config --libs libuv)
-L$DEVKITPRO/libnx/lib -lnx -lm
```

## CRITICAL: V8 ↔ libuv libc-symbol collision (SOLVED)

Both ports implement the same newlib gap-fillers. V8's
`v8_libbase.libc-horizon.o` (archive member of `libv8_monolith.a`) defines
exactly three symbols: `posix_memalign`, `pthread_sigmask`, `sysconf`.
libuv's `libuv-horizon-port.o` (force-linked single object) defines
`pthread_sigmask` and `sysconf` (among many others).

Because they are strong defs in two objects, the link fails with
`multiple definition of 'pthread_sigmask' / 'sysconf'`. Providing our own
`posix_memalign` does NOT help (V8's object is pulled as a unit anyway and then
all three collide).

### Fix: weaken the libc gap-fillers at the PACKAGE source (preferred)

The fillers are libc fallbacks, not port API, so both ports now mark them
**weak** — any strong definition (another port's or the embedder's) wins
silently and two ports filling the same gap never collide. This is fixed
upstream in the packages (`pacman-packages/switch`), so nx.js needs **no**
Makefile workaround:

- `switch-v8`: `v8/horizon-src/libc-horizon.cc` — the 3 fillers
  (`pthread_sigmask`, `sysconf`, `posix_memalign`) are tagged `HORIZON_WEAK`
  (`__attribute__((weak))`).
- `switch-libuv`: `libuv/PKGBUILD` — after compiling `libuv-horizon-port.o`, an
  `objcopy --weaken-symbol` pass (driven by `nm --defined-only --extern-only`)
  weakens **every** global in the object (whole-TU, future-proof: every symbol
  in that object is a newlib gap-filler).

- **`switch-skia`**: `skia/PKGBUILD` — `skia-horizon-port.o` defines `pread`,
  which also collides with libuv's `pread`. Same `objcopy --weaken-symbol` pass
  added after it is compiled.

**Action required:** rebuild + reinstall `switch-v8`, `switch-libuv`, and
`switch-skia` from `pacman-packages/switch` so the installed libs carry the weak
symbols (and Skia the fixed header layout). After that the validated link recipe
links with **zero** changes on the nx.js side. (Verified each objcopy step on the
installed objects: all globals become `W`, 0 strong globals remain.)

## Phase 1 progress — nx.js core compiles + links (PASS)

The nx.js native layer now builds on V8 + libuv (Cairo retained):

- `types.h` rewritten for V8/libuv (NX_SET_FUNC / NX_DEF_GET[SET] / NX_DEF_FUNC
  macros, nx_work_t over uv_work_t, nx_context_t with v8::Global handlers).
- `error.cc`, `async.cc`, `util.cc` ported (foundational layer).
- `main.cc` written: V8 isolate (single-threaded flags), libuv loop hosted in
  the appletMainLoop frame loop (uv_run NOWAIT -> PerformMicrotaskCheckpoint ->
  onFrame Global -> present), `$` bridge build, module registration, ES-module
  user entrypoint load, runtime.js eval, clean shutdown.
- Build system: `tools/embed-runtime.mjs` embeds runtime.js as a C byte array
  (`source/runtime_js.c`), replacing the `qjsc` bytecode step. Makefile compiles
  `.cc` (C++20, -fno-exceptions -fno-rtti), links the V8 group + libuv pkg-config
  + Cairo. Deleted: poll.c, thpool.c, wasm.c/.h, runtime.c, queue.h.
- The remaining ~26 modules are TEMPORARY `.cc` stubs (register nothing); real
  idiomatic-V8 ports land incrementally (Phase 1.4–1.7).

**Verified with the fixed packages installed:** a clean `make` builds
`nxjs.nro` (28 MB) with ZERO undefined symbols and NO weaken-bridge — the
package-source fixes (weak libc gap-fillers) resolve the collisions. The
temporary consumer-side bridge is fully retired.

Link-order note: V8 (`mman-horizon.cc` -> `jitCreate`/`jitTransitionToExecutable`)
and libuv (`uv_os_gethostname` -> `gethostname`) reference libnx symbols that
resolve late, so the Makefile appends an explicit `-L$(DEVKITPRO)/libnx/lib
-lnx` at the END of `LIBS` (after the V8 group + libuv) in addition to the
specs-provided `-lnx`. Without it the link fails with undefined `jitCreate` /
`gethostname`.

Build note: after deleting the old `.c` files, run `make clean` once — stale
`build/*.d` dependency files reference the removed `.c` paths and break an
incremental `make`.

## ✅ BASELINE PASSED ON HARDWARE (FW/Atmosphère, 2026-06-02)

First V8 + libuv + Cairo `nxjs.nro` ran on a real Switch. The `romfs/main.js`
smoke test rendered correctly (verified via screenshot): dark-blue clear, red
`fillRect`, green stroked `arc`, and FreeType/HarfBuzz text ("nx.js on V8 +
Cairo" 48px + "frame N" 24px), animating at 60fps (counter reached 535+),
stable, no crash. This validates end-to-end: V8 JIT evaluating the embedded
runtime + ES-module entrypoint, the libuv-hosted frame loop (onFrame → rAF),
and the full Canvas 2D path (Cairo) presenting to the framebuffer.

## BASELINE HARDWARE TEST (first bootable V8+Cairo nxjs.nro)

`make` produces a self-contained `nxjs.nro` (~33 MB, 0 undefined symbols) that
embeds `romfs/main.js` — a Canvas 2D smoke test (moving rect, stroked circle,
text). To test on hardware:

1. Copy `nxjs.nro` to `sdmc:/switch/nxjs.nro` (or anywhere on the SD card).
2. Launch via hbmenu. The runtime resolves `romfs:/main.js` first (embedded),
   so no separate `.js` is needed.
3. Expected: dark-blue screen, a red rect sliding L→R, a green circle, and
   "nx.js on V8 + Cairo" + a frame counter, at 60 fps.
4. Press `+` to exit cleanly to hbmenu.

Debug: errors/logs are written to `nxjs-debug.log` next to the NRO (stderr is
freopen'd there). If it crashes, check `sdmc:/atmosphere/crash_reports/`.

Boot-path TS fixes that were required (Phase 1.9):
- Removed the `import * as WebAssembly from './wasm'` + `def(WebAssembly,...)`
  in index.ts (wasm3 shim gone; V8 has native WebAssembly). The `./wasm` module
  is no longer imported at runtime (only `export type`).
- `dom-exception.ts` became a real polyfill (QuickJS-ng provided DOMException
  natively; V8 does not) and is `def()`'d as a global in index.ts.
- crypto is a BOOT SHIM (registers cryptoInit/cryptoKeyInit as no-ops so module
  load doesn't crash; all crypto ops throw "not implemented"). Full port TODO.
- audio stays a stub (its `$.audioInit` is lazy, inside the Audio constructor,
  so it doesn't run at module load).

## Applied to main.cc from the on-hardware findings

- **Memory-gated JIT** (the "trifecta" big one): `svcGetInfo(Total/Used)` →
  if free < ~300 MiB, init V8 `--jitless ... ` + `set_code_range_size_in_bytes(0)`;
  else full-JIT flags. Phase 1 (CPU/Cairo canvas) normally picks full JIT in
  both regimes; the gate matters once the Skia-GL canvas lands (Phase 2).
- **`horizon_mman_teardown()`** is now called after `V8::DisposePlatform()` —
  mandatory so a reload doesn't start with a corrupted address space.
- `socketInitialize()` already runs before the libuv loop is used (satisfies
  libuv's loopback-TCP self-pipe requirement; equivalent to
  `socketInitializeDefault` with our buffer config).
- Loop still driven by `appletMainLoop()` (matches existing nx.js applet
  behavior + clean `+` exit). The notes' "don't gate on appletMainLoop" caveat
  applies to title-redirect/full-mem demos; revisit if full-mem NSP shows
  early-exit.

## Module conversion status (Phase 1)

REAL idiomatic-V8 ports (building green, 26): error, async, util, wrap, main,
battery, applet, nifm, memory, window, gamepad, account, album, web, dns, fs,
tcp, udp, tls, url, compression, image, irs, font, dommatrix, canvas.

STUBS remaining (5): crypto (3700 lines — mbedtls WebCrypto), audio, fsdev, ns,
service.

software-keyboard.cc: the libnx swkbd inline callbacks are global C functions,
so the active keyboard is tracked in a file-global `current_kbd`; retained JS
callbacks are `Global<Function>` + a `Global<Object>` receiver. Callbacks fire
on the main thread during `swkbdInlineUpdate`, so V8 calls inside them are fine.

canvas.cc notes (the big one — full Canvas 2D on V8 + Cairo, ~2700 lines):
- All cairo/harfbuzz drawing bodies kept verbatim; only the JS boundary is V8.
- `ENTER_THIS` / `ENTER_ARGV0` macros replace `CANVAS_CONTEXT_THIS/_ARGV0`:
  they unwrap the context (info.This() or info[0]), run ensure_surface, and
  bail on zero-dim (no-op) or error. `get_doubles(info, args, n, offset)`
  replaces `js_validate_doubles_args`.
- `state->font` (was a JSValue) became `state->font_face` (nx_font_face_t*) —
  save/restore re-selects the cairo face from the pointer, no JS handle needed.
- canvas / context-2d / gradient all wrapped via nx::NewWrapped/Wrap with
  cooperative free fns; gradients are `cairo_pattern_t*` opaque handles.
- put/getImageData use ArrayBufferView/ArrayBuffer directly; `applyPath` calls
  back into JS via `$.applyPath`. toBuffer encode runs on the libuv threadpool.
- Was converted incrementally (helpers+cairo bodies first, then batches of
  bindings), compile-checking each batch — far more reliable than one rewrite.

canvas conversion plan (Phase 1.8d): canvas.h structs (`nx_canvas_t`,
`nx_canvas_context_2d_t` w/ the save/restore state stack) carry over; the 297
cairo calls in the body stay verbatim. Only the JS-binding shell changes: class
wrapping (-> nx::NewWrapped/Wrap), arg marshalling, ImageData ArrayBuffers,
fillStyle/strokeStyle RGBA tuples, gradient opaque handles, the `$.canvas*`
function registrations. main.cc's framebuffer path already expects
`nx_canvas_pixels/width/height` accessors + `nx_get_canvas` (currently the
stub). dommatrix/image/font helpers it calls are now all real.

image/irs/font notes:
- `image.cc` keeps the Cairo decode body (libpng/turbojpeg/webp ->
  cairo_image_surface). `image.h` now shares `nx_image_t`/`nx_get_image` (C++
  signature) with canvas + irs. `decode_jpeg` stays a non-namespaced C++
  symbol.
- `irs.cc`: `BGRA8(r,g,b,a)` is a libnx/switch MACRO — do NOT redefine it.
- `font.cc` keeps FreeType + HarfBuzz(OT) + cairo_ft body; getSystemFont copies
  the libnx shared font into an ArrayBuffer.

Networking is COMPLETE: dns + tcp + udp + tls all over libuv (uv_poll_t on raw
fds; tls = mbedtls retried per readiness). The fetch / WebSocket / HTTPS stack
is wired end-to-end once the runtime runs.

url.cc note: ada's `ada_c.h` has no `extern "C"` guard but ada.cpp exports the
symbols with C linkage, so url.cc wraps `#include "ada_c.h"` in `extern "C"`.
URL and its searchParams share one `nx_url_t`; both halves get a cooperative
weak finalizer (`free_url_half`/`free_params_half`) that frees only when the
other half is already gone.

dommatrix is deferred to the canvas/rendering phase (1.8): it's large (~900
lines) and Cairo-coupled (its `cairo_matrix_t` union + `nx_dommatrix_*` helpers
are used by canvas).

Sockets (poll.c → libuv): `tcp.cc` reimplements the deleted hand-rolled
`poll.c` watcher on **`uv_poll_t` over raw BSD socket fds** (NOT `uv_tcp_t`),
because the TS layer + the TLS module are fd-centric ((err,value) callbacks,
`read(cb,fd,buf)`/`write(cb,fd,buf)`). Pattern: non-blocking `socket()`,
`uv_poll_init_socket` + `uv_poll_start(UV_READABLE/UV_WRITABLE)`, do the
recv/send/accept in the poll cb, then `uv_poll_stop` + `uv_close` (free the op
in the close cb). `udp.cc` and `tls.cc` follow this same fd+uv_poll model.

Conversion notes:
- `album.cc` / `dns.cc` validate the libuv async path (`NX_INIT_WORK_T` →
  `nx_queue_async` → worker `do` → loop-thread `cb` resolving/rejecting), incl.
  ArrayBuffer ownership transfer and throwing from the after-cb.
- `irs` depends on `nx_get_image`/`nx_image_t` + Cairo — convert AFTER `image`.
- `fs` is large (~25 fns, all async) — convert as a unit using the album/dns
  pattern; strings/buffers must be copied into the work struct (worker thread
  can't hold v8 handles).
- Several modules (`fsdev`, `ns`) define libnx weak overrides / globals
  (`__nx_applet_exit_mode`) — keep those as plain C-linkage globals.

## Removed modules (native in V8)

- **uint8array** — the TC39 "Uint8Array to/from base64/hex" methods
  (`toBase64`/`toHex`/`fromBase64`/`fromHex`/`setFromBase64`/`setFromHex`) are
  native builtins in this V8 (confirmed: `Builtin_Uint8ArrayFromBase64`,
  `Builtin_Uint8ArrayPrototypeToBase64`, etc. in the monolith; shipped unflagged
  in V8 13.3+). Deleted `uint8array.cc`/`.h`, the `$.uint8arrayInit` binding, and
  the runtime side-effect import. The TS *type* declarations in
  `uint8array.ts` are kept (still `export type *`'d from index.ts).
- **wasm** — V8 ships native WebAssembly (deleted wasm3 earlier).

## Three-way integration PoC (V8 + libuv + Skia-GL) — PASS

A single C++ TU that initializes V8 (compile+run JS + microtask checkpoint),
libuv (`uv_loop_init`/`uv_run`), and Skia GPU (Ganesh GL: `GrGLInterfaces::
MakeEGL`, `GrDirectContexts::MakeGL`, `SkSurfaces`, `SkCanvas`/`SkPaint`, and
`SkShaper` text shaping) **compiles and links to a valid 36 MB AArch64 PIE
`.nro` with zero undefined symbols.** This is the de-risking proof that all
three packages coexist in one binary.

Cross-package symbol audit (all clean / collisions solved):
- V8 `chrome_zlib` ∩ Skia strong symbols: **0** (Skia uses system `-lz`).
- V8 monolith ∩ Skia strong symbols: **0**.
- libc gap-filler collisions (the ONLY conflicts), all fixed by weakening:
  - `pthread_sigmask`, `sysconf` — V8 `libc-horizon.o` ↔ libuv port object.
  - `posix_memalign` — V8 only (also weakened for safety).
  - `pread` — Skia port object ↔ libuv port object.

Full Skia-GL link stack (after the V8/libuv stack):
`skia-horizon-port.o` + `--start-group -lskia-gl -lskcms-gl -lskshaper-gl
-lskunicode_core-gl -lskunicode_libgrapheme-gl --end-group` + `-lEGL -lGLESv2
-lglapi -ldrm_nouveau -lfreetype -ljpeg -lpng -lwebp -lwebpdemux -lbz2 -lz`.
Compile Skia-using TUs with `-DSK_GL` and `-I $PORTLIBS_PREFIX/include/skia`
(post header-layout fix).

### Earlier workaround (no longer needed, kept for reference)

Before the package fix, the same effect was achieved on the consumer side by
copying `libv8_monolith.a`, extracting `v8_libbase.libc-horizon.o`, weakening
the 3 symbols with `objcopy`, and re-`ar`/`ranlib`-ing into a build-local
archive. Superseded by the package-source fix.

## On-hardware runtime findings (V8 + Skia + libuv "trifecta" demo)

> For the resulting **JIT-vs-canvas memory policy recommendation** (when to use
> full JIT vs. jitless, keyed on CPU vs. GPU canvas + memory regime), see
> **`JIT-CANVAS-POLICY.md`**.

The link-time PoC above proves the three packages *build* together. This
section captures what it took to make them actually *run* at 60 fps on real
hardware (FW 18.1.0), discovered building a standalone demo
(`pacman-packages/examples/trifecta`: libuv timer → JS `scene(t)` in V8 → Skia
draw). All of these are directly relevant to the nx.js runtime.

### THE BIG ONE: full-JIT V8 and the GPU (Mesa) stack contend for memory

The Switch runs homebrew in two memory regimes, and **nx.js ships in both** (NRO
= applet, NSP = application):

| canvas backend | applet (~137 MiB free) | application (~3 GiB) |
|---|---|---|
| **CPU** (cairo today, or Skia raster) | ✅ 60 fps, **full JIT** | ✅ 60 fps, full JIT |
| **GPU** (Skia Ganesh GL) | ⚠️ **needs jitless** (full JIT crashes) | ✅ 60 fps, full JIT |

Why GPU + full-JIT crashes in applet mode: V8's full-JIT path calls libnx
`jitCreate` for its code range; the floor is **64 MiB** and `jitCreate`
**dual-maps** it (rx + rw) → **~128 MiB real**. On top of the ~243 MiB baseline
that leaves ~9 MiB of the 137 MiB free — and **Mesa's GLSL shader compiler**
(`_mesa_glsl_builtin_functions_init`) then NULL-derefs on its first shader
compile (`GrGLCompileAndAttachShader`). Confirmed by isolation: standalone Skia
GL (no V8) renders fine at 137 MiB free; standalone full-JIT V8 (no GPU) runs
fine; only the *combination* in applet mode fails. This is a hard memory wall,
**not** a Mesa bug — and it cannot be tuned away (see the budget knob below).

**The fix for GPU canvas in applet mode: run V8 jitless.**
```c
v8::V8::SetFlagsFromString("--jitless --single-threaded --single-threaded-gc "
                           "--no-concurrent-recompilation --predictable");
create_params.constraints.set_code_range_size_in_bytes(0);   // no jitCreate
```
Jitless skips `jitCreate` entirely → frees the ~128 MiB → GPU Skia fits, 60 fps.
JS then runs in the interpreter (slower, but fine for scene/UI logic).

**Recommended nx.js policy — gate on MEASURED FREE MEMORY, not the NRO/NSP
format:**
```c
u64 total=0, used=0;
svcGetInfo(&total, InfoType_TotalMemorySize, CUR_PROCESS_HANDLE, 0);
svcGetInfo(&used,  InfoType_UsedMemorySize,  CUR_PROCESS_HANDLE, 0);
u64 free = total - used;
bool can_jit = (free > /* ~300 MiB: code arena + GPU/Mesa headroom */);
if (!can_jit) v8::V8::SetFlagsFromString("--jitless ...");
```
- **If nx.js keeps a CPU canvas (cairo, or Skia-CPU): just use full JIT always.**
  No Mesa, no conflict, 60 fps in both modes — simplest, and matches the current
  QuickJS+cairo behavior. The jitless dance is ONLY needed for a GPU canvas in
  applet mode.
- **If/when nx.js moves to a GPU (Skia GL) canvas:** full JIT in NSP, jitless in
  applet (or whenever free memory is tight).

### V8 code-arena runtime knob (switch-v8 ≥ 15.0.243-3)

```c
extern "C" void horizon_mman_set_code_budget(size_t wasm_headroom_mb,
                                             size_t max_code_mb);  // before V8 init
```
Default arena = 64 MiB JS code + 64 MiB WASM headroom. A non-WASM build can call
`horizon_mman_set_code_budget(0, 0)` to drop to the 64 MiB floor, reclaiming
~64 MiB real memory (useful in NSP for textures/heap). NOTE: it does **not**
let full-JIT GPU fit in applet mode (64 MiB floor × 2 dual-map = 128 MiB still
starves Mesa) — jitless is the only applet-GPU option.

### MUST call horizon_mman_teardown() before exit (or reload corrupts)

V8 on Horizon maps its arenas with manual `svcMapMemory`, which libnx's exit
does NOT unmap. If nx.js returns to hbloader (or relaunches) without releasing
them, the **next launch starts with a corrupted address space** and crashes
(`posix_memalign` → NULL during V8 init). Symptom: works on a fresh launch,
crashes on reload.
```c
extern "C" void horizon_mman_teardown(void);   // call after V8::Dispose, before return
```
Every `switch-v8` hello-v8 example calls this; nx.js's V8 shutdown path must too.

### V8 init specifics (Horizon)

- Use `v8::platform::NewSingleThreadedDefaultPlatform()` — the multi-threaded
  default spins worker threads whose abseil CondVar/pthread sync **faults** on
  Horizon.
- For full JIT, set `code_range_size_in_bytes(64 MiB)` and a modest heap via
  `ConfigureDefaultsFromHeapSize`; the package's arena logic caps at total/3.
- `InitializeICUDefaultLocation` is unnecessary (this V8 is built
  `v8_enable_i18n_support=false`).

### libuv runtime gotchas (all cost real debugging time)

- **`socketInitializeDefault()` is REQUIRED before using the loop.** libuv's
  `uv_default_loop()` sets up its async self-pipe, which on Horizon is a
  **loopback-TCP socket pair** (libnx has no anonymous pipes / `socketpair`).
  Without sockets initialized, loop creation crashes.
- **`uv_timer_start(t, cb, timeout, repeat)`: `repeat == 0` is a ONE-SHOT
  timer** (fires once). Use a non-zero repeat for a frame loop. (This silently
  capped a "frame loop" at one frame and looked like a hang/early-exit.)
- **Do NOT gate the run loop on `appletMainLoop()`** in title-redirect/full-mem
  mode — it can return false immediately and end the loop at frame 1. Drive the
  loop with `uv_run` and exit on input (e.g. `+`) or an explicit `uv_stop()`.
- TCP servers work, but the Switch `bsd:` service caps concurrent connections at
  ~25 (firmware session-pool limit) and does NOT deliver same-process 127.0.0.1
  loopback *accepts* (in-process listen→connect→accept never completes).
- `uv__cloexec` was a real port bug (now fixed in switch-libuv): libnx `fcntl`
  returns `EOPNOTSUPP` as a POSITIVE value for `F_SETFD`, which stock libuv read
  as failure and closed every accepted socket. If you see "server accepts one
  connection then drops all others," ensure switch-libuv ≥ 1.52.1-2.

### Skia packaging note (switch-skia ≥ 149-3)

`include/core/SkColorSpace.h` (a CORE header pulled in by surfaces/images) does
`#include "modules/skcms/skcms.h"`. Earlier packages shipped only
`modules/skshaper` + `modules/skunicode`, so anything touching color spaces
failed to compile ("modules/skcms/skcms.h: No such file"). Fixed in 149-3 (it
now installs `modules/skcms`). Ensure that version is installed.

### Skia + freetype/harfbuzz link

Link **system `-lharfbuzz`** (grouped with `-lfreetype`) to satisfy
`switch-freetype`'s autohinter `hb_*` references. Skia keeps its OWN bundled,
ICU-free HarfBuzz internally — do NOT try to make Skia use system harfbuzz
(system hb pulls ICU, defeating Skia's no-ICU design).

### EGL vs the libnx console

EGL/GL and the libnx console are mutually exclusive on the single default
NWindow. A GPU path has no `consoleInit`; log to SD instead. (nx.js already owns
the framebuffer/EGL, so this is mostly informational.)

### Reference

Full working demo + a README with all of the above:
`devkitPro/pacman-packages/examples/trifecta` (both CPU and GPU backends, 60 fps
on hardware). Build via `aarch64-none-elf-pkg-config` + the documented link
groups.

## ada (URL parser) is now a package — drop the vendored copy

`switch-ada` (devkitPro/pacman-packages) now packages the Ada WHATWG URL parser,
so nx.js should stop vendoring `source/ada.cpp` / `source/ada.h` / `source/ada_c.h`
and consume the package instead.

- Installs `libada.a` + `ada.h` + `ada_c.h` + an `ada.pc` under
  `$PORTLIBS_PREFIX`. Consume via `aarch64-none-elf-pkg-config ada` (or just
  `-lada` / `#include <ada_c.h>`).
- Built C++20, `-fno-exceptions -fno-rtti` (matches nx.js's existing ada build
  flags), so the ABI lines up.
- **Version bump caveat:** the package is **v3.4.4**; nx.js currently vendors
  **2.9.2**. The C API (`ada_c.h`) is backward-compatible — 3.x only *adds*
  functions (e.g. `ada_get_version`, `ada_get_version_components`) — but it IS a
  major version jump, so smoke-test URL parsing after switching. nx.js's hand-
  written `ada_c.h` is also superseded by upstream's official `ada_c.h` (now a
  release asset), which the package installs.
- To migrate: remove the three vendored files + their Makefile compile entry,
  add `switch-ada` to the build deps, and pull flags from `pkg-config ada`.

## crypto.c -> crypto.cc (full WebCrypto port to V8) — DONE

The last stub module is fully ported. `source/crypto.cc` reimplements the entire
~3700-line QuickJS WebCrypto module against the raw V8 API, keeping every mbedtls
/ libnx work-body verbatim.

Key porting decisions:
- **Async payload lifetime:** added an optional `nx_data_dtor data_dtor` to
  `nx_work_t`. When set, the framework calls it (instead of `free()`) after
  `after_work_cb`. New macro `NX_INIT_WORK_T_CPP(type)` `new`s the payload and
  registers `delete`, so async structs can safely hold `v8::Global<>` members.
  `image.cc` was retrofitted to use it (it `new`'d its struct but was being
  `free()`'d before).
- **CryptoKey wrapping:** replaced the QuickJS class-id + opaque + JSClassDef
  finalizer with `nx::NewWrapped` + `nx::Wrap<nx_crypto_key_t>(... finalizer)`.
  `cryptoKeyNew*` return wrapped objects; the TS side `proto()`s them onto
  `CryptoKey.prototype`; getters (`type`/`extractable`/`algorithm`/`usages`)
  unwrap via `nx::Unwrap`. The finalizer frees mbedtls structs + Resets the two
  cached `Global<Value>`s.
- **Cached algorithm/usages:** `nx_crypto_key_t.algorithm_cached` /
  `usages_cached` are now `v8::Global<v8::Value>` (was `JSValue`). calloc'd key
  structs get placement-`new`'d Globals; the GC finalizer Resets them.
- **Result buffers:** the old `JS_NewArrayBuffer(..., free_array_buffer, ...)`
  (transfer ownership of a malloc'd buffer) maps to a local `nx_ab_take()`
  helper using `ArrayBuffer::NewBackingStore(buf, len, free-deleter)`.
- **Retained inputs across threads:** async structs hold `Global<Value>` for the
  key/algorithm/data/signature args (keeps ArrayBuffer backing stores alive on
  the uv worker). Owned C strings (ECDSA hash name) are `strdup`'d and freed in
  the cb; `algorithm_params` malloc'd on the JS thread, freed in the cb.
- **getRandomValues quota error:** throws a `QuotaExceededError` DOMException via
  the global `DOMException` constructor when >65536 bytes (falls back to a
  plain error if unavailable).
- **strcasecmp/md-type:** `nx_crypto_get_md_type` lives in the anon namespace,
  forward-declared at top; used by worker `_do` fns (pure C, thread-safe).

Builds clean (only the pre-existing cosmetic `.To()` `warn_unused_result`
warnings); `nxjs.nro` ~33MB, **0 undefined symbols**. `romfs/main.js` now runs a
`crypto.subtle` self-test on boot (SHA-256 KAT, getRandomValues, AES-CBC round
trip, HMAC sign/verify) and renders PASS/FAIL on screen; FTP'd to the Switch for
hardware verification.

All native modules are now real V8 ports — no stubs remain.

### Hardware result + FJCVTZS (JSCVT) fix

First on-hardware run of the crypto build crashed: Atmosphère `Undefined
Instruction`, opcode `1e7e0008` = `FJCVTZS W8, D0` (ARMv8.3-A JSCVT). The Switch
Tegra X1 (Cortex-A57/A53) is ARMv8.0-A and lacks JSCVT; V8's arm64 JIT emitted it
for a JS `ToInt32` fast path because its CPU-feature probe on Horizon wrongly
reported JSCVT available (no HWCAP/`/proc` on Horizon → bad default in
`base::CPU` / `CpuFeatures::ProbeImpl`). Pure-Canvas baseline avoided it; the
number-heavy crypto self-test triggered it.

Fixed in the **switch-v8 package** (CpuFeatures probe patched so JSCVT is never
enabled on the Horizon target; pkgrel bump + rebuild/reinstall). Handoff write-up:
`devkitPro/pacman-packages/V8-JSCVT-CRASH.md`. After relinking nx.js against the
rebuilt V8, the full-JIT `nxjs.nro` **runs on hardware**: Canvas renders and the
`crypto.subtle` self-test reports all PASS (SHA-256 KAT, getRandomValues,
AES-CBC round-trip, HMAC sign/verify). Full JIT confirmed working on hardware.

## Phase 1.11 — TS cleanup + version reporting (DONE)

Aligned the TypeScript runtime + native version object with the V8 era:

- **Dead wasm `$` bindings removed.** Deleted the 13 `wasm*` entries from
  `packages/runtime/src/$.ts` (the `// wasm.c` block) plus the `Memory`/
  `MemoryDescriptor` import and the `WasmModuleOpaque`/`WasmInstanceOpaque`/
  `WasmGlobalOpaque` types in `internal.ts`. WebAssembly is native V8.
- **`src/wasm.ts` deleted** (the orphaned wasm3 shim — not imported by index.ts).
  Replaced with **`src/wasm-types.ts`**, a types-only `declare`d module that
  `build.mjs` consumes to emit `declare namespace WebAssembly { ... }` in the
  published `index.d.ts` (needed because the bundle uses `no-default-lib`, so
  `WebAssembly` is not pulled from `lib.dom`). `build.mjs` entry repointed
  `./src/wasm.ts` -> `./src/wasm-types.ts`.
- **Versions interface (`switch/index.ts`)**: removed `quickjs`, `wasm3`, and
  `ada`; added `v8`. The native `Switch.version` object in `main.cc` was
  rewired to set the full set again — `cairo, freetype2, harfbuzz, hos, libnx,
  mbedtls, nxjs, pixman, png, turbojpeg, v8, webp, zlib, zstd` plus lazy
  `ams`/`emummc` accessors (SPL-backed, `SetNativeDataProperty`). `ada` dropped
  (the package symbol `ada_get_version` isn't linked — we still compile the
  vendored `ada.cpp`, which lacks it; revisit if/when switching to switch-ada).
  Added the needed includes to `main.cc` (ft2build, hb, mbedtls/version, png,
  webp/decode, zlib, zstd).
- **MemoryUsage interface** rewritten from the QuickJS shape (mallocSize,
  objCount, …) to the V8 `HeapStatistics` fields the new `memory.cc` actually
  returns (totalHeapSize, usedHeapSize, heapSizeLimit, mallocedMemory, …).
  Doc comments de-QuickJS'd.
- **source-map.ts**: de-QuickJS'd the CallSite comment; broadened the eval/REPL
  filename sentinel to handle V8's `<anonymous>` as well as the old `<input>`.
  `Error.prepareStackTrace` + CallSite were already V8-compatible.

Validation: `tsc --noEmit` clean; `build.mjs` (d.ts bundle) + `bundle.mjs`
(runtime.js) succeed; `index.d.ts` now declares `readonly v8` and no longer
`quickjs`/`wasm3`/`ada`. NRO relinks with 0 undefined symbols (~33.5MB).

## DEFERRED: port the host conformance harness (nxjs-test) to V8

`packages/runtime/test/` is still entirely QuickJS-era and currently cannot
build against the V8 source. Deferred because it needs a **host V8 + libuv**
which aren't installed (brew has v8 14.8.178 + libuv 1.52; the Switch package is
pinned at 15.0.243 — minor API skew possible).

What needs doing (full map already scouted):

1. **CMakeLists.txt**: change every `${NX_SOURCE_DIR}/*.c` -> `*.cc`; **drop**
   `poll.c`, `thpool.c`, `uint8array.c`, `wasm.c` (all deleted); **add**
   `memory.cc`, `wrap.cc`, `software-keyboard.cc` (init symbol is
   `nx_init_swkbd`). Replace the QuickJS-ng + wasm3 FetchContent with host V8 +
   libuv (keep mbedtls). Drop `qjs`/`m3` link libs; add v8/libplatform + uv.
   Compat include dir stays first.
2. **src/main.c -> main.cc**: mirror `source/main.cc`'s V8 model — isolate +
   `NewSingleThreadedDefaultPlatform`, the `$` init_obj bridge, the 28
   `nx_init_*` calls (add `memory`; the harness must NOT call `uint8array`/
   `wasm`). Replace the custom `nx_poll`+`nx_process_async`+
   `nx_process_pending_jobs` trio with `uv_run(loop, UV_RUN_NOWAIT)` +
   `iso->PerformMicrotaskCheckpoint()`. Keep host bits: argv contract
   `nxjs-test <runtime.js> <fixture.js>`, read runtime.js from FILE and run as a
   classic script, run the fixture as an ES module, PSA crypto init, CA-cert
   preload, host font override (`$.getSystemFont`), SIGINT, TAP via printf to
   stdout, exit code from `had_error`. No `appletMainLoop`/`padUpdate` on host
   (pass plusDown=false to onFrame).
3. **src/stubs.c -> C++** with the V8 signature
   `void nx_init_x(v8::Isolate*, v8::Local<v8::Object>)`; same 12 Switch-only
   no-op modules: account, album, applet, audio, battery, fsdev, gamepad, irs,
   nifm, ns, service, swkbd.
4. **src/compat/**: `switch.h` mostly reusable (libnx type stubs + real mbedtls
   AES/SHA + `randomGet`); add any new libnx symbols the `.cc` modules
   reference. **`compat/poll.h` is now obsolete** (no `source/poll.h`; types.h
   includes `<uv.h>`). `compat/switch/services/ssl.h` still needed by `tls.cc`.
5. **Test runner is reusable as-is**: `conformance.test.ts`,
   `build-fixtures.mjs`, `src/tap.ts`, `src/tap-parser.ts`, `vitest.config.ts`
   only depend on the `nxjs-test <runtime.js> <fixture.js>` stdout/TAP contract.
   34 fixtures in `fixtures/*.ts` are engine-agnostic.

Note: the stale March `build/nxjs-test` binary + the `build/`/`build-sdl/`
CMake caches reference QuickJS/wasm3 and should be wiped before reconfiguring.

The AGENTS.md "Host-Platform Test Binary" section still documents the
QuickJS-era layout and should be refreshed when this lands.

## Teardown ordering bug — second launch crashed hbmenu/bsdsocket (FIXED)

Symptom: nxjs ran fine on the FIRST launch, but relaunching (returning to
hbmenu and selecting it again) crashed. Crash reports showed the faults were
NOT in nxjs but downstream, after nxjs exited:
- `bsdsocket` sysmodule (Program ID 0100000000000012): "User Break" (internal
  assertion in the system socket service).
- `nx-hbmenu` (via hbloader 010000000000100d): Data Abort, fault address 0x0.

Root cause: `main.cc` teardown called `horizon_mman_teardown()` (which
`svcUnmapMemory`s V8's manually-mapped arenas — carved from the stack/virtmem
region) BEFORE closing libnx services (`socketExit`/`plExit`/`romfsExit`) and
freeing mbedtls/FreeType/`nx_ctx`. Running a service Exit in a half-dismantled
address space corrupted the bsdsocket session and left leaked V8 arena aliases,
which poisoned the NEXT process's address space (hbmenu null-deref). First run
worked because the address space was still pristine.

Fix: reordered teardown so ALL heap/service cleanup (FreeType, mbedtls frees,
`splExit`, `free(nx_ctx)`, then `plExit`/`romfsExit`/`socketExit` in reverse
init order) runs while the address space is intact, and `horizon_mman_teardown()`
is the VERY LAST call before `return 0` — matching the hello-v8 reference
ordering. Verified on hardware: repeated launches now succeed.

Rule of thumb: `horizon_mman_teardown()` must be dead last. Nothing that touches
the heap or a libnx service may run after it.

## Second-launch crash on CLEAN exit — hbloader heap corruption (V8 mman bug)

Symptom: launch an nx.js app, exit cleanly with `+`, then launch ANY homebrew →
hbmenu crashes (until a full console restart). NOT a force-quit; the app reaches
full teardown.

Symbolized the hbmenu crash from `sdmc:/hbmenu.nro` (raw aarch64 disasm at the
crash PC `nx-hbmenu + 0xf6b34`): it's **newlib malloc/free free-list block
splitting** (`str x1, [x2, #8]` writing a block header at a ~null split point).
hbmenu faults walking a **corrupted free list** — its heap was corrupted before
it even started, by the previous V8 process.

Root cause is in the **switch-v8 package** (`horizon-src/mman-horizon.cc`): V8's
data arena slabs are `memalign`'d from the shared (hbloader) heap and aliased
into a stack-region reservation via `svcMapMemory`; at teardown they're
`svcUnmapMemory`'d and `free()`'d back into the heap. That map/unmap/free
lifecycle leaves the hbloader heap's allocator metadata inconsistent, so the
next process crashes in malloc. nx.js already reaches `horizon_mman_teardown()`
on clean exit — the corruption is inside the arena lifecycle, not a missing
teardown. Full write-up handed to the V8 package:
`devkitPro/pacman-packages/V8-RELAUNCH-HEAP-CORRUPTION.md`.

nx.js-side hardening kept regardless (these are correct independent of the mman
fix): teardown closes all libuv handles + their socket fds before
`socketExit()`; the main loop breaks to teardown on `Switch.exit()`
(`is_running`) and on `+` even when no frame handler is registered (so
finished scripts like the test runner exit cleanly instead of force-quit).

## Networking bugs fixed → full test suite GREEN (1091/1093, 0 failed)

Debugging the on-device test app (apps/tests) + an isolated diagnostic app
(apps/net-debug) surfaced and fixed several real runtime bugs. Final result:
**1091 passed, 0 failed, 2 skipped** (the 2 are intentional test.skip), clean
exit, no crash.

Bugs fixed:
- **`NX_GetBufferSource` rejected valid EMPTY buffers** (util.cc): V8 backs a
  zero-length ArrayBuffer/view with a null data pointer, which callers read as
  "not a buffer". Return a non-null sentinel (size 0) for empty-but-valid
  buffers. Fixed the `http://` gzip+chunked path ("expected ArrayBuffer" in the
  DecompressionStream transform on the chunked terminal 0-length chunk).
- **TLS fd double-close** (tls.cc + tcp.ts): the TS layer `$.close`d the raw fd
  while the native TLS context still had a uv_poll_t on it → bsdsocket User
  Break (and on TLS-failure paths the GC finalizer closed it again). Now the
  native TLS context SOLELY owns its fd: added `$.tlsClose(ctx)` that uv_close's
  the poll FIRST then closes the fd; handshake-failure tears down natively; TS
  transfers fd ownership (i.fd=-1, tracks tlsFd) and never $.close's a TLS fd.
- **TLS read gated on socket-readability** (tls.cc): mbedtls reads whole TLS
  records and buffers decrypted bytes internally, so the fd often is NOT
  readable when app data is available → the poll never fired → read hang. Now
  attempt mbedtls_ssl_read immediately when a read is queued, polling only on
  WANT_READ.
- **TLS single `active_op` → concurrent read+write hang** (tls.cc): fetch writes
  the request while the response readable stream is already pulling, so a READ
  and a WRITE are in flight on the same connection. The single active_op was
  overwritten (write clobbered read) → the read's promise never settled → all
  HTTPS body reads hung. The connection now tracks read_op + write_op
  separately; the shared poll drives BOTH (write first to flush, then read) and
  recomputes interest as each finishes. (Same class of bug the per-fd registry
  fixed for plain TCP.)
- **socket.readable cancel()** (tcp.ts): added a cancel handler that closes the
  socket so a terminated body stream (Content-Length reached) tears down the
  connection instead of leaving a keep-alive read pending.

Also confirmed NOT nx.js bugs: `example.com`'s cert chain doesn't validate
against the Switch's built-in CA set (54 CAs load fine; letsencrypt/digicert/
github/google all verify) — it was just a bad test target. The earlier
source-map "not a valid URL" masking + assert.not.equal were test-harness fixes.

apps/net-debug is a kept diagnostic (DNS / TCP / TLS verify / fetch / decompress
probes writing to sdmc:/switch/net-debug.log).

## 2nd-launch crash after socket use — libuv self-pipe leak (FIXED)

Symptom: a socket-using app (fetch/DNS/TCP/TLS) ran + exited cleanly, but
launching ANY homebrew afterward crashed with a bsdsocket User Break
(bsdsocket + 0xe7064) + an hbmenu malloc free-list fault. A non-socket app
(hello-world) relaunched fine.

Root cause: libuv's async/signal self-wakeup "pipe" is a loopback TCP socket
pair on Horizon (libnx has no anonymous pipes — see switch-libuv
`horizon-port.c` pipe() emulation). Those self-pipe socket fds are closed by
`uv_library_shutdown()`. The switch-libuv port disables the usual
`__attribute__((destructor))` that would call it at libc teardown (it runs
AFTER socketExit and faults), and REQUIRES the embedder to call
`uv_library_shutdown()` explicitly while the socket layer is still up. We never
did → the self-pipe bsd sockets leaked → `socketExit()` tore down bsdsocket
with live sessions → the sysmodule faulted on the NEXT launch. Only socket-using
apps created the self-pipe (via the async watcher / threadpool), which is why
hello-world was unaffected.

Fix: call `uv_library_shutdown()` in main.cc teardown, immediately before
`plExit`/`romfsExit`/`socketExit` (i.e. while the socket layer is up).

Debugging notes / dead ends ruled out along the way (all NOT the cause):
- TLS GC-finalizer use-after-free (freeing the ctx while a uv_close was pending)
  — was a real latent bug, fixed (want_free deferral), but not THIS crash.
- TCP closing the fd synchronously before the poll's async uv_close — real bug,
  fixed (close the fd inside the uv_close callback), but not THIS crash.
- Socket config (BsdServiceType_Auto + large buffers) — NOT the cause; kept
  Auto (bsd:s is needed to bind privileged ports e.g. 80).
- Worker-thread DNS racing libnx's global `sm` session — plausible in theory,
  but empirically DNS on the threadpool relaunches cleanly once the self-pipe
  leak is fixed, so DNS stays async on the threadpool.

The V8 mman arena leak (separate, earlier) was fixed in the switch-v8 package;
this self-pipe leak is the socket-specific complement on the libuv side.
