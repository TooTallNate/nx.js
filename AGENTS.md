# AGENTS.md — nx.js

Guide for AI agents contributing to this repo.

## What is nx.js?

A JavaScript runtime for **Nintendo Switch homebrew**, built on QuickJS + libnx + cairo. It implements Web APIs (Canvas 2D, Fetch, Crypto, URL, EventTarget, etc.) so you can write Switch apps in JS/TS.

## Repo Structure

```
source/          — C code (native modules compiled for aarch64 Switch)
packages/
  runtime/       — TypeScript runtime (bundled via QuickJS bytecode compiler)
    src/$.ts     — Native binding types (the `$` object bridges JS↔C)
    src/index.ts — Entry point, registers all globals
    test/        — Host-platform test binary & TAP conformance tests
  nro/           — .nro builder (Switch homebrew executable format)
  nsp/           — .nsp builder
  create-nxjs-app/ — Scaffolding tool
apps/            — Example apps (each is a standalone pnpm workspace package)
Makefile         — Cross-compiles C code via devkitPro toolchain
```

## How Native Modules Work

Every feature follows the same pattern:

### C side (`source/foo.c` + `source/foo.h`)

```c
#include "types.h"

static JSValue nx_foo_do_thing(JSContext *ctx, JSValueConst this_val,
                               int argc, JSValueConst *argv) {
    // Implementation using libnx or other C libraries
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
    JS_CFUNC_DEF("fooDoThing", 1, nx_foo_do_thing),
};

void nx_init_foo(JSContext *ctx, JSValueConst init_obj) {
    JS_SetPropertyFunctionList(ctx, init_obj, function_list,
                               countof(function_list));
}
```

Header (`source/foo.h`):
```c
#pragma once
#include <quickjs.h>
void nx_init_foo(JSContext *ctx, JSValueConst init_obj);
```

Then in `source/main.c`:
- `#include "foo.h"` at the top
- `nx_init_foo(ctx, nx_ctx->init_obj);` in the init block (~line 680, alphabetical)

### JS side (`packages/runtime/src/foo.ts`)

```typescript
import { $ } from './$';
import { def } from './utils';
import { EventTarget } from './polyfills/event-target';

export class Foo extends EventTarget {
    constructor() {
        super();
        $.fooInit();
        addEventListener('unload', $.fooExit);
    }
}
def(Foo); // registers class name for toString/instanceof
```

- The `$` object contains ALL native function bindings (defined in C `function_list`s)
- Add types for new `$` functions in `packages/runtime/src/$.ts`
- Register in `packages/runtime/src/index.ts`
- Use `assertInternalConstructor` for classes that shouldn't be user-constructible
- Use `proto()` for classes that return a native object from C (like Image)

### Async Work Pattern

For expensive operations (decoding, crypto, etc.), use the thread pool:

```c
// In work_cb (runs on thread pool — NO JS API calls allowed):
static void my_work_cb(nx_work_t *req) {
    my_data_t *data = (my_data_t *)req->data;
    // Do heavy work here
}

// In after_work_cb (runs on main thread — JS API calls OK):
static JSValue my_after_work_cb(JSContext *ctx, nx_work_t *req) {
    my_data_t *data = (my_data_t *)req->data;
    // Return result to JS
    return JS_NewArrayBuffer(...);
}

// Queue it:
return nx_queue_async(ctx, req, my_work_cb, my_after_work_cb);
```

See `source/async.c` for the implementation, `source/image.c` and `source/crypto.c` for examples.

## Key Types & Macros (from `source/types.h`)

- `nx_context_t` — per-runtime context (thread pool, work queue, rendering mode, etc.)
- `NX_DEF_GET(obj, "name", getter_fn)` — define a getter
- `NX_DEF_GETSET(obj, "name", get_fn, set_fn)` — define getter + setter
- `NX_DEF_FUNC(obj, "name", fn, arg_count)` — define a method
- `countof(x)` — array length macro

## File Loading Pattern

Files are loaded via `fetch()` with `romfs:/` URLs (Switch ROM filesystem) or regular paths:

```typescript
// In Image class (good reference for loading resources):
fetch(url)
    .then(res => res.arrayBuffer())
    .then(buf => $.imageDecode(this, buf))
```

The `$.entrypoint` gives the base URL for resolving relative paths.

### RomFS mounts (`romfs:` vs `nxjs:`)

`main.cc` mounts two RomFS devices:

- **`nxjs:`** — the nx.js NRO's OWN embedded RomFS (holds the runtime's source
  map, `nxjs:/runtime.js.map`). Always mounted. The embedded runtime is run
  under the name `nxjs:/runtime.js` so its stack frames symbolicate.
- **`romfs:`** — "the app". For a standalone app this is the same NRO's RomFS
  (so existing `romfs:/asset` references work). For a **bootstrap launch**
  (`argv[1]` is an app `.nro`), it is the launched app's RomFS instead.

### Entrypoint resolution (`argv[1]`)

`main.cc` resolves the user entrypoint as:

1. If `argv[1]` is set (bootstrap launch — a thin launcher hands nx.js the app):
   - `*.nro` → mount its embedded RomFS as `romfs:` and run `romfs:/main.js`.
     The RomFS is not at file offset 0, so `mount_nro_romfs()` parses the NRO
     header + asset header to find the RomFS offset, then calls
     `romfsMountFromFsdev(argv[1], romfs_offset, "romfs")`.
   - otherwise (typically `*.js`) → run `argv[1]` directly.
2. Else (standalone): mount self as `romfs:`, run `romfs:/main.js`; fall back to
   `<argv0>.js` next to the `.nro` on the SD card.

The entrypoint resolution (`resolve_entrypoint()`) runs **early in `main()`, before
V8 init** — not because the app code runs early, but because `nxjs.ini` (below)
lives next to the entrypoint and its V8/heap settings must apply before
`V8::Initialize()`/`Isolate::New`.

### Configuration (`nxjs.ini`)

An optional INI file located **next to the entrypoint** (`romfs:/nxjs.ini` for a
standalone/bootstrap app, `<dir>/nxjs.ini` for a loose `.js`). Parsed very early
in `main()` with the bundled `source/vendor/ini.h` (inih), using plain `fopen` —
the JS `fetch`/`readFileSync` layer doesn't exist yet, and the `[v8]`/`[memory]`
settings must be applied before the isolate is created. Lives in
`source/config.cc`/`config.h` (`nx_config_t` on `nx_context_t`).

```ini
[v8]
jit   = auto                       ; auto (regime-based) | on | off
flags = --max-old-space-size=256   ; appended AFTER the runtime's default V8 flags

[memory]
heap_limit = 256MiB                ; KiB/MiB/GiB or raw bytes; clamped to fit

[renderer]
mode = auto                        ; auto | cpu | gpu (gpu falls back to raster on init fail)

[console]                          ; on-screen console / terminal styling
font_size      = 22
cursor_style   = bar               ; block | underline | bar
background     = #002b36
foreground     = #839496
black = #073642   red = #dc322f   ...  bright_white = #fdf6e3

[socket]                           ; field-level overrides on the regime base (lean/full)
tcp_tx_buf_size = 256KiB
tcp_rx_buf_size = 256KiB
tcp_tx_buf_max_size = 1MiB
tcp_rx_buf_max_size = 1MiB
udp_tx_buf_size = 9KiB
udp_rx_buf_size = 42KiB
sb_efficiency = 6
num_bsd_sessions = 3
service_type = auto                ; auto | user | system

[runtime]                          ; consumed by the SLIM bootstrap launcher (not the runtime)
version = ^1                       ; semver requirement for the shared runtime NRO
```

- **Effective** (post-clamp) values are exposed to JS as `$.config`
  (`{ jit, heapLimit, renderer, v8Flags, socket:{…}, loaded }`).
- **Every value that can't be honored is logged** to `nxjs-debug.log` as a
  `[config] … not honored: <reason>` line (clamped heap, invalid value, GPU
  init fallback, socket reservation too big, etc.). A missing file is silent.
- `jit` is honored verbatim (no clamp): JIT works in applet mode for CPU
  rendering; only **applet + GPU + JIT** is known to crash (jitCreate starves
  Mesa) — that combo is warned about but still attempted.
- The host `nxjs-test` reads **no** INI; its `build_init_object` exposes a
  default `$.config`. Keep that mirror in sync if you change `$.config`'s shape.
- `[runtime]` is read by the **slim bootstrap launcher**, not the runtime. The
  runtime's parser recognizes + silently ignores it (the same `nxjs.ini` ships
  inside a slim app's RomFS and is read by both).

### Slim apps / bootstrap launchers (`bootstrap/`)

An app can be packaged **slim** (default — shares one on-SD runtime) or **fat**
(self-contained). This applies to BOTH `.nro` (`@nx.js/nro`) and `.nsp`
(`@nx.js/nsp`) outputs.

`bootstrap/` holds the **shared** launcher logic (pure-C, libnx only, NO
V8/Skia), with two flavor subdirs that differ only in the final "launch" step:

- `bootstrap/source/` — shared: `resolve.c` (read `[runtime] version` from the
  app's own `romfs:/nxjs.ini`, scan `sdmc:/nx.js/nxjs-v<full-version>.nro`,
  semver-match the highest), `match.c` (specifier parse + compare),
  `ui.c` (on-screen error + wait-for-`+`), `vendor/{ini.h,semver.c}`. A future
  "download the runtime if missing" step slots in here.
- `bootstrap/launcher-nro/` → `bootstrap.nro`. Resolves the runtime, then
  `envSetNextLoad(<runtime>, "\"<runtime>\" \"<self.nro>\"")` and exits; hbloader
  loads the runtime with `argv[1]` = the slim NRO, whose RomFS the runtime mounts
  via `mount_nro_romfs(argv[1])`.
- `bootstrap/launcher-nsp/` → `hbl.nso` + `hbl.npdm`. A patched **nx-hbloader
  forwarder** that runs as the slim NSP's exefs `main`. `envSetNextLoad` is
  hbloader-only and unavailable to an installed title, so the forwarder instead
  *is* an hbloader: it resolves the runtime, then loads that NRO directly
  (`svcMapProcessCodeMemory` + the homebrew `ConfigEntry[]` ABI) with
  `argv` = `"<runtime>" "nsp:"`. The loaded runtime sees `envIsNso()==false`
  (forwarded homebrew, not the title's NSO) and the `"nsp:"` marker tells it to
  mount the **installed title's own** RomFS (the app's files) via
   `romfsMountFromCurrentProcess` (fs cmd 200), then run `romfs:/main.js`.
   Vendored from switchbrew/nx-hbloader (MIT) + Skywalker25/Forwarder-Mod; only
   needs rebuilding on a major libnx/firmware ABI change.
   - **No-runtime error path**: when `nx_resolve_runtime` fails, the forwarder
     can't just abort (`diagAbortWithResult`) — it renders the shared on-screen
     error UI (`bootstrap/source/ui.c` `nx_fail_no_runtime`) so the user sees a
     real message (+ the future runtime-download UI lives here). Two gotchas
     this requires: (1) the forwarder's `__libnx_initheap` malloc arena must be
     **16 MiB** (upstream nx-hbloader uses 16 KiB) — `consoleInit`'s
     `framebufferCreate` allocates from malloc and *hangs* on a tiny heap; and
     (2) the forwarder's minimal `__appInit` doesn't bring up the display/input
     stack, so `nx_show_error_and_exit` runs the libnx `__appInit` order
     (`sm`/`applet`/`hid`/`time`+`__libnx_init_time`/`__nx_win_init`) before
     delegating to the shared UI.
   - **Clean exit from the error screen**: an installed title that just
     `svcExitProcess()`s makes the OS show "software was closed". The proper
     applet self-exit handshake in libnx's `_appletCleanup` is gated on
     `(envIsNso() && __nx_applet_exit_mode==0) || __nx_applet_exit_mode==1` —
     and forwarded homebrew has `envIsNso()==false`, so the forwarder sets
     **`u32 __nx_applet_exit_mode = 1`**. The shared `ui.c` calls a weak
     `nx_ui_exit()` after `+` (default = `consoleExit` + return, correct for the
     hbloader-launched NRO launcher); the forwarder provides a *strong*
     `nx_ui_exit()` that replicates `__libnx_exit` by hand (it links
     `-Wl,-wrap,exit`, whose `__wrap_exit` aborts, so `exit()` is unusable):
     teardown → `appletExit()` (registers `_appletExitProcess` as the exit func)
     → `__nx_exit(0, envGetExitFuncPtr())` jumps to it. All four matrix cases
     (NRO/NSP × runtime present/missing) verified on-device.

- **Fat**: `nxjs-nro --fat` / `nxjs-nsp --fat` (or `NXJS_FAT=1`) embeds the full
  runtime (the ~40 MB NRO / ~21 MB NSO). `--slim` / `NXJS_SLIM=1` are accepted
  no-ops (slim is the default). Both default changes are **breaking** for
  existing `nxjs-nro`/`nxjs-nsp` scripts.
- **Build**: `make -C bootstrap/launcher-nro` and `make -C bootstrap/launcher-nsp`
  (devkitPro; `jq` derives the runtime major from
  `packages/runtime/package.json` → the baked `^major` default). Each launcher's
  Makefile is tiny and `include`s `bootstrap/common.mk` (shared devkitPro build
  scaffold; compiles `bootstrap/source/*` into both). CI builds both, uploads
  them, and the release job copies `bootstrap.nro` → `packages/nro/dist/` and
  `hbl.nso`/`hbl.npdm` → `packages/nsp/dist/`.
- **Match logic** (`bootstrap/source/match.c`) is split from libnx so it's
  host-unit-tested: `bootstrap/test/run.sh` (no Switch needed).
- **Prerelease note**: the vendored `semver.c` compares purely by precedence and
  does NOT apply node-semver's "prereleases excluded from non-prerelease ranges"
  rule, so `^1` DOES match `1.0.0-beta.N` — intentional during the v1 beta.
- **`nsp:` runtime branch**: `resolve_entrypoint()` in `source/main.cc` detects
  `argv[1] == "nsp:"` and mounts `romfs:` via `romfsMountFromCurrentProcess`
  (the installed title's data storage), instead of `mount_nro_romfs` (NRO path)
  or `romfsMountSelf` (standalone). `nxjs:` (runtime's own files) is unaffected.
- SD-card convention: shared runtimes live at
  `sdmc:/nx.js/nxjs-v<full-version>.nro`; multiple versions may coexist. The same
  runtime NRO serves both slim NRO and slim NSP apps.

### ES module `import` (static + dynamic)

`source/module.cc` (shared by the device runtime and the host test binary, so
they never drift) implements native ES module resolution for the entrypoint and
its imports:

- **Static `import`** and **`await import()`** resolve specifiers as URLs (via
  `ada`) against the importing module's URL: relative (`./`, `../`),
  absolute-path (`/x.js`), and absolute-URL (`romfs:`/`sdmc:`/`nxjs:`/`file:`)
  specifiers work; **bare specifiers throw** (no node_modules / import maps).
- Resolution is **synchronous** (`fopen`/`read_file`), so only mounted devoptab
  schemes work. `http(s):`/`data:` imports are not supported (would need an
  async loader). JSON / asset (synthetic) modules are also not implemented yet.
- A module **cache** keyed by resolved URL gives referential stability (a shared
  dependency imported via multiple paths is one instance) and handles cycles.
  `import.meta.url` is each module's resolved URL; `import.meta.main` is true
  only for the entrypoint.
- Apps are normally esbuild-bundled (imports resolved at build time), so this is
  additive — it enables unbundled multi-file apps, the REPL, and app-level lazy
  `import()`. Top-level await works (V8 native; a rejected entry graph is routed
  to the error path).
- `main.cc` calls `nx_init_modules(iso)` once after `Isolate::New`,
  `nx_run_entry_module(...)` to run the entrypoint, and `nx_modules_teardown()`
  before isolate dispose. The host test binary mirrors these calls.

## Build System

- **C code**: `Makefile` using devkitPro/libnx toolchain (aarch64, cross-compiled)
- **JS/TS**: pnpm workspaces, esbuild for app bundling
- **Package manager**: pnpm 8.x
- **CI**: GitHub Actions — builds, tests
- **Cannot build locally** unless you have devkitPro installed. Don't try to `make` in CI or sandboxes.

### Adding C libraries

- For link libraries: add to `LIBS` in `Makefile` (e.g., `-lmpg123`)
- For single-header libs: place in `source/vendor/` and include directly
- Memory alignment matters on Switch: use `memalign()` for DMA buffers, `armDCacheFlush()` for audio mempools

## Versioning & Changesets

nx.js **follows [semver](https://semver.org/)** as of v1. Choose the bump type that matches the change:

- **`patch`** — bug fixes and other backwards-compatible changes.
- **`minor`** — new, backwards-compatible features.
- **`major`** — breaking changes to the public API or runtime behavior.

```markdown
---
"@nx.js/runtime": patch
---

feat: description of what changed
```

- The repo is currently in changesets **beta prerelease mode** (`.changeset/pre.json`), so releases publish as `X.Y.Z-beta.N`. A `major`/`minor`/`patch` changeset still picks the underlying version bump; the `-beta.N` suffix is applied automatically while in pre mode.
- All packages in `@nx.js/runtime`, `@nx.js/nro`, `@nx.js/nsp`, and `create-nxjs-app` are version-locked (see `.changeset/config.json` `fixed` array)
- If your change touches `source/` (C code), the changeset should include `@nx.js/runtime`

## Example Apps

Each app in `apps/` follows this structure:
```
apps/my-app/
  package.json    — scripts: build (esbuild), nro, nsp
  src/main.ts     — entry point
  romfs/          — files bundled into the Switch ROM filesystem
```

Package.json template:
```json
{
  "name": "my-app",
  "version": "0.0.0",
  "private": true,
  "scripts": {
    "build": "esbuild --bundle --sourcemap --sources-content=false --target=es2022 --format=esm --outdir=romfs src/main.ts",
    "nro": "nxjs-nro",
    "nsp": "nxjs-nsp"
  },
  "devDependencies": {
    "@nx.js/nro": "workspace:^",
    "@nx.js/nsp": "workspace:^",
    "@nx.js/runtime": "workspace:^",
    "esbuild": "^0.17.19"
  }
}
```

The `screen` global is the main display (1280×720). Use `screen.getContext('2d')` for drawing.

**When creating a new example app**, use `apps/hello-world/` as a template. Copy the entire directory and modify it — this ensures you include all necessary structure and config files (`.gitignore`, `tsconfig.json`, `package.json`, `romfs/`, etc.).

## Git Workflow

- Branch from `main`
- PRs reviewed by @TooTallNate
- CI must pass (Build, Test, WebAssembly Conformance)
- **Use `git worktree`** if working on multiple branches simultaneously — never share a working directory between parallel tasks

## Common Pitfalls

- `pnpm install` hangs on prompts in non-interactive shells → use `pnpm install < /dev/null` or `--no-frozen-lockfile`
- Can't call JS APIs from thread pool worker callbacks — only in `after_work_cb`
- Switch memory is limited — be mindful of large allocations
- `romfs:/` paths use forward slashes, colon after scheme
- **Globals like `setTimeout`, `setInterval`, `clearTimeout`, `clearInterval` are NOT available inside `packages/runtime/src/`** — they're only registered as globals in `index.ts` for user code. Within the runtime package itself, import them: `import { setInterval, clearInterval } from './timers';`
- **Gamepad button mapping** is NOT standard Web Gamepad API order. Use `@nx.js/constants` `Button` enum (e.g. `Button.A`, `Button.B`). The order is: B=0, A=1, Y=2, X=3, L=4, R=5, ZL=6, ZR=7, Minus=8, Plus=9, StickL=10, StickR=11, Up=12, Down=13, Left=14, Right=15
- **`stub()` does NOT mean unimplemented.** Methods marked with `stub()` in TypeScript (from `./utils`) are placeholders for type generation only. At runtime, the C side overwrites them on the prototype via `NX_DEF_FUNC()` or `NX_DEF_GET()`/`NX_DEF_GETSET()`. If you see `stub()`, check the corresponding C file's `nx_init_*` or `*_init_class` function — the real implementation is there. Only `throw new Error('Method not implemented.')` means actually not implemented.
- **`nxjs.ini` must be read before `V8::Initialize()`** (for `[v8]`/`[memory]`), so entrypoint resolution is hoisted to the top of `main()` and the INI is parsed with plain `fopen` (via `source/vendor/ini.h`), NOT the JS `readFileSync`. The `[v8] jit` setting drives `can_jit`, which **couples** the V8 flags (`--jitless`), the heap `reserve` (180 vs 48 MiB), AND the code-range size (64 MiB vs 0) — override `can_jit` once, don't touch those three independently. Socket overrides are clamped so a bad value can't make `socketInitialize` (which `diagAbortWithResult`s on failure) brick startup. Any non-honored value logs `[config] … not honored: <reason>` to `nxjs-debug.log`.
- **`Application.self` must use `$.selfNroPath`, NOT `$.argv[0]`.** In slim modes `argv[0]` is the *shared runtime* NRO, so keying `self` off it makes `self.name` report `"nx.js"` instead of the app. `build_init_object` (both `source/main.cc` and the host mirror) sets `$.selfNroPath` per launch mode: the app's own NRO path for a standalone/slim NRO (`argv[0]` standalone, `argv[1]` for a slim `.nro` launch), or `null` for an installed title (fat/slim NSP, and the `argv[1]=="nsp:"` marker) so `nsAppNew(null)` resolves via the process `ProgramId` + `nsGetApplicationControlData`. Verified on-device across the fat/slim × NRO/NSP matrix.
- **devkitPro's `switch_rules` resolves `APP_TITLE`/`APP_AUTHOR` at include time** — set them (and `APP_TITLEID`, wired via `NACPFLAGS += --titleid=...`, not a positional `nacptool` arg) **before** `include $(DEVKITPRO)/libnx/switch_rules` in `bootstrap/common.mk`, or the `?=` is a no-op and the launcher NACP gets `Unspecified Author` / `PresenceGroupId 0x0`. A slim app inherits the bootstrap launcher's NACP author + title id, so this is what makes a slim app's `Application.self` (author/id) match the fat build (whose base is `nxjs.nro`, with the title id set in the root `Makefile`).
- **Classes whose constructor `return`s a substitute object (e.g. `Screen`, `Image`) must NOT use `#private` fields/methods for shared state.** `Screen`'s constructor does `const c = proto($.canvasNew(...), Screen); _.set(c, {}); return c;` — the returned native object never had the class's private members installed, so calling a `#method` on the `screen` instance throws `"Receiver must be an instance of class Screen"`. Use the `createInternal()` WeakMap (`_`) + a module-level free function instead (see `ensureContext()` in `screen.ts`).
- **Third-party libs that read `navigator`/`window`/`document` at module-eval time can break runtime startup.** `@xterm/headless` computes `isNode ? 'node' : navigator.userAgent` at import, and `navigator.userAgent`'s getter walks `Application.self` → ns init (`addEventListener`, `$.argv`) — not all ready that early, so init threw on device AND host. Fix: a `bundle.mjs` esbuild `inject` (`xterm-process-shim.js`) supplies a module-scoped `process` ({title}) so xterm's `isNode` is true and it never reads `navigator`. The shim is injected only into modules referencing free `process` (xterm), not a real global.
- **Feed a terminal CRLF, not bare LF.** xterm (and real terminals) treat `\n` as line-feed only (cursor down, same column), so `console.log`'s `\n` staircases. `Terminal.write()` normalizes lone LF → CRLF.
- **xterm cell colors have THREE modes — handle truecolor (`isBgRGB`/`isFgRGB`) separately.** `getBgColor()`/`getFgColor()` return a *palette index* (0–255) in palette mode but a *packed `0xRRGGBB`* in RGB mode (e.g. kleur's `bgRgb()`/`rgb()` emit `48;2;r;g;b`). A packed RGB int is almost always > 255, so if you only handle the 0–255 palette ranges it falls through to the fallback (this made `console.warn`/`error` backgrounds render white). The terminal renderer checks `isBgRGB()`/`isFgRGB()` first and unpacks the bytes. Note `isBgRGB()`/`isFgRGB()` return `boolean`, while sibling cell predicates like `isBold()`/`isFgDefault()` return `number` (0/1) — don't `!== 0` the booleans.
- **Canvas terminal cell backgrounds must use integer pixel bounds — AND the cell advance itself must be a whole pixel.** The monospace advance (`measureText('M').width`) is fractional. Rounding each cell's edges (`round(x*cw)`..`round((x+1)*cw)`) isn't enough on its own: with a fractional `cw`, adjacent cells alternate floor/ceil widths and a glyph drawn at its own fractional advance (e.g. the FULL BLOCK `█`) leaves thin background seams. `terminal.ts` snaps `charWidth = Math.round(measureText('M').width)` so every cell is exactly `cw` px and both `fillRect` backgrounds and glyphs tile seamlessly.
- **The console is themeable via `console.options` (or `new Console(opts)`), or declaratively via the `[console]` section of `nxjs.ini`.** `ConsoleOptions extends TerminalOptions` (`theme`, `fontSize`, `lineHeight`, `scrollback`, `cursorStyle` `'block'|'underline'|'bar'`, `cursorOpacity`). For the *global* `console`, assign `console.options` BEFORE the first log — the `Terminal` is created lazily in `#getTerminal()`, and the setter drops any existing terminal so the next output rebuilds it (scrollback is reset). The `[console]` ini keys (`font_size`, `cursor_style`, `background`/`foreground`/`cursor`, `black`..`bright_white`) are parsed in `source/config.cc` into `nx_console_config_t`, exposed on `$.config.console` (a `TerminalOptions`-shaped object with a `theme` sub-object), and the global `Console`'s constructor seeds `#options` from `$.config.console` when no explicit opts were passed — an explicit `console.options =` still overrides. Keep `source/main.cc`'s `$.config.console` builder and the host mirror (`packages/runtime/test/src/main.cc`, empty object) in sync. The renderer honors the **full** xterm ANSI palette: `terminal.ts` resolves a 16-entry `#palette` from the theme's `black`..`brightWhite` fields over the `ANSI_COLORS` defaults (don't re-hardcode `ANSI_COLORS` in `#cellColor` — index `#palette`). See `apps/console-theme` for a Solarized Dark example. The public option type uses a **locally-defined `ConsoleTheme`** (a subset of xterm's `ITheme`); see the next bullet for why.
- **Never let the runtime's public type surface reference a `node_modules` type (e.g. `@xterm/headless`'s `ITheme`/`Terminal`).** `build.mjs` bundles `src/index.ts` into a single ambient-global `dist/index.d.ts` (starts with `/// <reference>`, no top-level `import`). If any exported type references an external module type, dts-bundle-generator emits a top-level `import { … } from '@xterm/headless'` at the top of `index.d.ts`, which turns the whole file into a **module** — and then the runtime's ambient globals (`Switch`, `Response`, `ReadableStream`, `TextEncoder`, …) stop being declared globally, so EVERY downstream `@nx.js/*` package (and the docs/Vercel build) fails to typecheck with `Cannot find name 'Response'` etc. Mirror external types locally (as `ConsoleTheme` does for `ITheme`) and type any getter that returns a raw vendor instance loosely (`Terminal.terminal: unknown`). Guard: `grep '^import' packages/runtime/dist/index.d.ts` must be empty, and `pnpm build` from the repo root must pass all packages.
- **DX: the libnx PrintConsole is initialized BEFORE running `runtime.js`** (`source/main.cc`) so that if runtime.js throws during evaluation, `print_js_error()`'s on-screen output (exception + stack) is visible instead of a bare "Runtime initialization failed". The full error is also in `sdmc:/switch/nxjs-debug.log`.

## Host-Platform Test Binary (`nxjs-test`)

The `packages/runtime/test/` directory contains a **unified host-platform build** of the nx.js runtime (`nxjs-test`) that compiles the real C source files against host system libraries, with libnx stubbed out. This enables running JS/TS tests on macOS/Linux without Switch hardware.

### Architecture

- **Real source modules** compiled from `source/`: async, canvas, compression, crypto, dns, dommatrix, error, font, fs, image, tcp, tls, udp, url, util, window, wrap. ada is linked from the `switch-ada` host lib (`/opt/host/ada`), not compiled here.
- **Stubbed modules** (no-op `nx_init_*` in `src/stubs.cc`): account, album, applet, audio, battery, fsdev, gamepad, irs, memory, nifm, ns, service, swkbd, web
- **Compat headers** in `src/compat/` provide libnx type/function stubs, real AES/SHA via mbedtls, host system CA certificates for TLS
- **60 FPS event loop** with real thread pool, networking, and async operations

### When modifying native C code (`source/`)

When you add or change a C source file in `source/`, you MUST update the test binary:

1. **New portable module** (uses standard C libs, not libnx): Add the `.cc` file to `NX_SOURCES` in `packages/runtime/test/CMakeLists.txt`. Add the `nx_init_*` call in `packages/runtime/test/src/main.cc` under the "Real modules" section.

2. **New Switch-only module** (uses libnx APIs): Add a no-op `nx_init_*` stub in `packages/runtime/test/src/stubs.cc`. Add the `nx_init_*` call in `packages/runtime/test/src/main.cc` under the "Stubbed modules" section.

3. **New libnx types/functions** referenced by compiled modules: Add stubs to `packages/runtime/test/src/compat/switch.h`.

4. **New linked library**: Add to the `target_link_libraries` and corresponding `pkg_check_modules` in `CMakeLists.txt`.

### TAP Conformance Tests

Test fixtures in `packages/runtime/test/fixtures/*.ts` run in **both** the nxjs-test binary (QuickJS) and Chrome (via Playwright), with TAP output compared assertion-by-assertion to verify conformance.

```
fixtures/*.ts → esbuild bundle → fixtures/build/*.js
                                         │
                         ┌───────────────┴───────────────┐
                         ▼                               ▼
                nxjs-test binary                Chrome (Playwright)
                (TAP on stdout)              (TAP from console.log)
                         │                               │
                         └───────────┬───────────────────┘
                                     ▼
                     vitest conformance.test.ts
                     (parse TAP, compare assertion-by-assertion)
```

**Writing a test fixture:**

```typescript
import { test } from '../src/tap';

test('my feature', async (t) => {
    t.equal(1 + 1, 2, 'math works');
    t.ok(true, 'truthy');
    t.deepEqual([1, 2], [1, 2], 'arrays match');
    t.throws(() => { throw new Error('boom'); }, 'boom', 'error message matches');
});
```

Available assertions: `t.ok`, `t.notOk`, `t.equal`, `t.notEqual`, `t.deepEqual`, `t.throws`, `t.doesNotThrow`, `t.match`, `t.pass`, `t.fail`.

**Bug fix policy:** When fixing a bug, add a regression test fixture (or add assertions to an existing fixture) that would have caught the bug. The test must pass in both nxjs-test and Chrome.

### Building and running tests locally

```bash
# Build the test binary (one-time, re-run after source/ changes)
cd packages/runtime/test
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run the conformance tests (from repo root)
pnpm --filter @nx.js/runtime test
```

System dependencies needed: `cmake`, `pkg-config`, `libcairo2-dev`, `libfreetype-dev`, `libharfbuzz-dev`, `libpng-dev`, `libturbojpeg0-dev`, `libwebp-dev`, `zlib1g-dev`, `libzstd-dev`, plus Playwright Chromium (`pnpm --filter @nx.js/runtime exec playwright install chromium`).


## Verification workflows (device + host)

These are the proven loops for verifying native (`source/`) and runtime
(`packages/runtime/src/`) changes. Read this before iterating — it captures the
shortcuts that make the dev cycle fast.

### Rebuilding `nxjs.nro` (the device runtime)

The runtime is embedded into `nxjs.nro` as a byte array (`source/runtime_js.c`).
The full chain after a change:

```bash
# 1. Bundle the TS runtime (only needed if you changed packages/runtime/src/):
pnpm --filter @nx.js/runtime bundle          # -> packages/runtime/runtime.js
# 2. Embed runtime.js as the C byte array (only if runtime.js changed):
node tools/embed-runtime.mjs packages/runtime/runtime.js source/runtime_js.c
# 3. Compile + link the NRO (needs devkitPro; can build locally on macOS):
DEVKITPRO=/opt/devkitpro make -j4            # -> nxjs.nro / nxjs.elf
```

- If you only changed `source/*.cc`, skip steps 1–2 (just `make`).
- If you only changed TS, you still need all three steps.
- `runtime_js.c` is byte-encoded; `strings nxjs.elf` won't show JS source —
  use it to confirm a rebuild happened, not to grep JS.
- The build is reproducible locally with the devkitPro toolchain at
  `/opt/devkitpro` (V8/Skia portlibs installed). Code layout can differ from the
  CI/published binary if the `switch-v8` package version differs (matters only
  for symbolizing a *published* crash — see below).

### On-device testing with `hello-world` as a throwaway

`apps/hello-world` is the fastest device test vehicle. Pattern:

1. Write a throwaway repro into `apps/hello-world/src/main.ts` (or place files
   directly in `apps/hello-world/romfs/` for unbundled tests — `romfs/` is
   gitignored). Have it log results to a file on the SD card so you can read
   them over FTP (console output is easy to miss; a log file is reliable):
   ```ts
   const LOG = 'sdmc:/switch/dbg.log';
   let buf = '';
   const log = (...a:any[]) => { buf += a.join(' ')+'\n';
     try { Switch.writeFileSync(LOG, buf); } catch {}; console.log(...a); };
   ```
2. Build + package the app NRO (the app embeds the runtime from the repo-root
   `nxjs.nro` via `@nx.js/nro`'s `../../../nxjs.nro`):
   ```bash
   cd apps/hello-world && pnpm build && DEVKITPRO=/opt/devkitpro pnpm nro
   ```
   - If your repro writes `romfs/main.js` directly (unbundled / multi-file
     module test), SKIP `pnpm build` (don't let esbuild overwrite it) and just
     run `pnpm nro`.
3. Upload + reset the log, then ask the user to run it and report back:
   ```bash
   curl -s --netrc -T hello-world.nro "ftp://192.168.1.249/switch/hello-world.nro"
   printf "" | curl -s --netrc -T - "ftp://192.168.1.249/switch/dbg.log"   # reset
   ```
4. After the user runs it, fetch the log:
   ```bash
   curl -s --netrc --ftp-method nocwd "ftp://192.168.1.249/switch/dbg.log" -o /tmp/dbg.log
   ```
5. **ALWAYS restore the throwaway afterward**: `git checkout -- apps/hello-world/`
   and delete any temp `romfs/` files you added. Never commit hello-world hacks.

**Application vs applet mode matters** for memory/JIT/GPU paths: application mode
(~3 GiB grant) runs full JIT + GPU canvas; applet mode (~380 MiB) runs jitless +
raster. The user chooses how they launch it — ask which mode to test, and have
the repro log `Switch.memoryUsage()` / read `[v8]` lines from
`sdmc:/switch/nxjs-debug.log` when memory behavior is relevant.

### FTP to the Switch console

The Switch runs an FTP server (e.g. ftpd/sys-ftpd). Access via `curl --netrc`
(credentials live in `~/.netrc`; anonymous is rejected):

```bash
# Upload an app NRO:
curl -s --netrc -T myapp.nro "ftp://192.168.1.249/switch/myapp.nro"
# Download a file (use --ftp-method nocwd for reliability):
curl -s --netrc --ftp-method nocwd "ftp://192.168.1.249/switch/foo.log" -o /tmp/foo.log
# List a directory:
curl -s --netrc "ftp://192.168.1.249/atmosphere/crash_reports/"
# Delete a file:
curl -s --netrc -Q "DELE /switch/foo.log" "ftp://192.168.1.249/"
```

Useful locations on the SD card:
- `/switch/` — apps + any logs you write via `Switch.writeFileSync('sdmc:/switch/...')`.
- `/switch/nxjs-debug.log` — the runtime's stderr (the `[v8] mem_total=... regime=... mode=...`
  startup line + any `fprintf(stderr, ...)` you add temporarily).
- `/atmosphere/crash_reports/*.log` — Atmosphère crash reports (see below).

### Diagnosing a device crash (Atmosphère crash reports)

A hard crash writes `/atmosphere/crash_reports/<id>_<programid>.log`. To read it:
1. Fetch the newest report over FTP (sort by timestamp).
2. Key fields: `Exception Info` (Type/Address), the `Address: ... (nxjs + 0xXXXX)`
   offsets, and the **Stack Dump** (ASCII often contains the abort message, e.g.
   `[FatalOOM] JavaScript OOM: CALL_AND_RETRY_LAST`).
3. Symbolize the `nxjs + 0xOFFSET` values with addr2line against a matching
   `nxjs.elf`:
   ```bash
   /opt/devkitpro/devkitA64/bin/aarch64-none-elf-addr2line -e nxjs.elf -f -C 0xOFFSET
   ```
   - The crash report's `Module Id` is the build-id; compare to your elf's
     (`aarch64-none-elf-readelf -n nxjs.elf | grep -i 'build id'`). If they
     differ, a local rebuild from the *same source* usually still symbolizes our
     own functions correctly, but V8-internal offsets may be slightly off. To
     symbolize a **published** binary exactly, rebuild on natecube with the same
     `switch-v8` package (see below); verify the match by confirming the `brk`
     opcode (`d4200000`) sits at the crashing offset in both NROs.
   - Common crash signatures seen: `OS::Abort` ← `Utils::ReportApiFailure`
     ("Empty MaybeLocal" = an unguarded `.ToLocalChecked()`); `FatalOOM`
     ("CALL_AND_RETRY_LAST" = V8 JS-heap exhaustion); `OS::Abort` ←
     `FatalNoSecurityImpact`; Data Abort in `MarkingBarrier`/`SetOldGenerationPageFlags`
     (V8 heap reservation exceeds what the Horizon mman can commit).

### natecube: host conformance harness (`nxjs-test`) + matching-toolchain builds

`nxjs-test` is the host-platform build of the runtime (compiles the real
`source/*.cc` against host libs, libnx stubbed) used to run the TAP conformance
fixtures and compare them to Chrome. It needs the CI toolchain (clang-19/lld-19
+ `/opt/host` V8/Skia/ada), which lives in the `nxdbg` Docker container on the
`natecube` host.

```bash
# The repo is bind-mounted at /work in the nxdbg container.
ssh natecube "docker exec nxdbg bash -lc 'cd /work && <cmd>'"
```

Notes / gotchas learned:
- The container is **x86_64**; install host node as the x64 build if missing.
- `git` needs `git config --global --add safe.directory /work` (dubious
  ownership) after a fresh container.
- node/pnpm are NOT baked into the image — CI installs them per-run
  (`actions/setup-node` + `pnpm/action-setup`). Install on demand if needed.
- Some apt dev headers (e.g. `libturbojpeg0-dev libjpeg62-turbo-dev`) may be
  missing in a fresh container and are needed to compile `image.cc`; CI installs
  them via apt.
- To **symbolize a published crash**, check out the published tag/commit in
  `/work`, rebuild `nxjs.nro` there (same `switch-v8` as CI), and addr2line.
- Build + run the host harness:
  ```bash
  cd packages/runtime/test
  cmake -B build -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19
  cmake --build build -j8                       # -> build/nxjs-test
  # Full conformance (needs Playwright Chromium):
  pnpm --filter @nx.js/runtime test
  # Run ONE fixture directly (no Chrome) — use ABSOLUTE paths; line-buffer
  # stdout because the binary idles until the fixture calls Switch.exit():
  node packages/runtime/test/build-fixtures.mjs        # build fixtures/*.ts -> fixtures/build/*.js
  stdbuf -oL ./packages/runtime/test/build/nxjs-test \
      "$(pwd)/packages/runtime/runtime.js" \
      "$(pwd)/packages/runtime/test/fixtures/build/<name>.js"
  ```
  A fixture that exits non-zero / segfaults: the harness loses block-buffered
  stdout, so a real failure can masquerade as "empty output, all fixtures fail".
  Run it directly with `stdbuf -oL` to see the actual TAP + exit code.
- The CI `pacman-packages` image SHA is pinned in `.github/workflows/ci.yml`
  (both the Build and Test jobs). When the V8/Skia portlib is rebuilt, the user
  provides a new image digest; bump both references and (optionally) re-create
  the natecube `nxdbg` container on the new image.

### General loop discipline

- Prefer a small, deterministic on-device repro (hello-world) over re-running
  the full conformance/app suite — the full suite depends on flaky public
  network endpoints (e.g. `echo.websocket.org`).
- After ANY native change, keep `source/main.cc` and the host
  `packages/runtime/test/src/main.cc` in sync (and add new shared logic to a
  common `.cc` compiled by both, rather than mirroring — see `source/module.cc`).
- Build artifacts (`nxjs.nro`, `nxjs.elf`, `packages/runtime/runtime.js`,
  `apps/*/*.nro`, `apps/*/romfs/main.js*`) are gitignored — never commit them.
