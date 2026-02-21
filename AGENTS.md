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
  nro/           — .nro builder (Switch homebrew executable format)
  nsp/           — .nsp builder
  create-nxjs-app/ — Scaffolding tool
apps/            — Example apps (each is a standalone pnpm workspace package)
test/canvas/     — Canvas 2D conformance tests (host-platform, not Switch)
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

## Build System

- **C code**: `Makefile` using devkitPro/libnx toolchain (aarch64, cross-compiled)
- **JS/TS**: pnpm workspaces, esbuild for app bundling
- **Package manager**: pnpm 8.x
- **CI**: GitHub Actions — builds, tests, canvas conformance
- **Cannot build locally** unless you have devkitPro installed. Don't try to `make` in CI or sandboxes.

### Adding C libraries

- For link libraries: add to `LIBS` in `Makefile` (e.g., `-lmpg123`)
- For single-header libs: place in `source/vendor/` and include directly
- Memory alignment matters on Switch: use `memalign()` for DMA buffers, `armDCacheFlush()` for audio mempools

## Versioning & Changesets

**⚠️ nx.js is NOT following semver yet. ALL changesets MUST be `patch`. No `minor` or `major` until stable 1.0.**

```markdown
---
"@nx.js/runtime": patch
---

feat: description of what changed
```

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
