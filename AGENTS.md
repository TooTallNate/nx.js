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

