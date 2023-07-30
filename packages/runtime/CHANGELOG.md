# nxjs-runtime

## 0.0.12

### Patch Changes

- Add `Switch.writeFileSync()` function ([`7ac3cca`](https://github.com/TooTallNate/nx.js/commit/7ac3cca46f9042399c8164ede4eceacb82c0f4bc))

- Add `Blob` ([`9c1b3d1`](https://github.com/TooTallNate/nx.js/commit/9c1b3d1e19f55285b7cff2b487221a045419c954))

- Fix Canvas `putImageData()` ([`8d7552b`](https://github.com/TooTallNate/nx.js/commit/8d7552bc30089becddbca2f1b2799fb44f11f88a))

- Fix Canvas `getImageData()` ([`c10a105`](https://github.com/TooTallNate/nx.js/commit/c10a105e0ccbe3004d51be65bb61c94cf7547943))

- Add Canvas `clip()` ([`af7a606`](https://github.com/TooTallNate/nx.js/commit/af7a606b1c2ec7f747f602b1374efd30f75a65e2))

- Add Canvas `roundRect()` ([`45321a6`](https://github.com/TooTallNate/nx.js/commit/45321a654995c45cb43baf72b388ee2d4d768e11))

- Better organization for polyfills ([`31dd7ad`](https://github.com/TooTallNate/nx.js/commit/31dd7ad2ecb83808da659df684a0c7713fc2e797))

- Add Canvas `save()` and `restore()` ([`34f2756`](https://github.com/TooTallNate/nx.js/commit/34f275647e3e556ac9cda82e7504112ba0f4dbe6))

- Add Canvas `strokeRect()` ([`2050801`](https://github.com/TooTallNate/nx.js/commit/20508014f63147b817d7b0615f5c02270e99864e))

- Add initial `fetch()` implementation. Includes globals: ([`f4ecc23`](https://github.com/TooTallNate/nx.js/commit/f4ecc23a8db1311ad73130646e180a0e028c1cde))

  - `Headers`
  - `Request`
  - `Response`
  - `fetch()`

- Add support for selecting fonts with modifications (weight, etc.) ([`67e97ce`](https://github.com/TooTallNate/nx.js/commit/67e97ce384a8c5a2d1a4fce8ea5cb52033ba2278))

## 0.0.11

### Patch Changes

- Add `ErrorEvent` polyfill ([`af2a932`](https://github.com/TooTallNate/nx.js/commit/af2a9329a575642ea7a5d547cd906af03dd406e7))

- Add Canvas `beginPath()`, `closePath()`, `fill()`, `stroke()`, `moveTo()`, `lineTo()`, `rect()` ([`a54f605`](https://github.com/TooTallNate/nx.js/commit/a54f60509d618b5128cd29e11f62fdc2348ee3d7))

- Move all dependencies to dev, since they get bundled ([`a4ea4f2`](https://github.com/TooTallNate/nx.js/commit/a4ea4f2134d6afc4f8149120f03b0ace88b7d623))

- Set app title to `nx.js` and add logo to `.nro` file ([`d4b1996`](https://github.com/TooTallNate/nx.js/commit/d4b19961b3799fe3f9725a5670329f11a8673452))

- Add Canvas `lineJoin`, `lineCap`, and `lineDashOffset` ([`26ee360`](https://github.com/TooTallNate/nx.js/commit/26ee36002169b356787e6629490e9eaf834ed5e9))

- Add Canvas `strokeStyle` ([`c0ba341`](https://github.com/TooTallNate/nx.js/commit/c0ba34116470bf79f9594c2de0db00f930c39cc6))

- Add `clientX` and `clientY` to touch events ([`dcc0519`](https://github.com/TooTallNate/nx.js/commit/dcc0519dd234d827255ffc05c08637c05c023ada))

- Add Canvas `getTransform()`, `bezierCurveTo()`, and `quadraticCurveTo()` ([`eb6ae49`](https://github.com/TooTallNate/nx.js/commit/eb6ae49b99a8e4e546c5ac9e76469d3ea28c55ea))

- Prevent further code execution upon initialization error ([`254ae9c`](https://github.com/TooTallNate/nx.js/commit/254ae9cfeea64acad08cf5707d239dbc2d9ab1c1))

- Add Canvas `setTransform()`, `resetTransform()`, `getLineDash()`, `setLineDash()`, and `lineWidth` getter ([`1e15670`](https://github.com/TooTallNate/nx.js/commit/1e156707875245e23f3d9d9ef3c15171cd6be209))

- Add `TextEncoder` polyfill and use better `TextDecoder` implementation ([`fe7fba0`](https://github.com/TooTallNate/nx.js/commit/fe7fba019ab22e11d6beabdd492fd7741ed5eed2))

- Add Canvas `arc()`, `arcTo()`, and `ellipse()` ([`6bf58b5`](https://github.com/TooTallNate/nx.js/commit/6bf58b582b6036879bd5afb20d83a1bbb2fd2ff7))

- Add initial `poll()` implementation ([#15](https://github.com/TooTallNate/nx.js/pull/15))

  - Adds `Switch.connect()`
  - Adds `Switch.read()`
  - Adds `Swtich.write()`

- Add Canvas `transform()` ([`8afa279`](https://github.com/TooTallNate/nx.js/commit/8afa279e3f51f4aae1c318f81ed8fcf736a23173))

- Add canvas context `rotate()`, `translate()` and `scale()` ([`674cc05`](https://github.com/TooTallNate/nx.js/commit/674cc05fd2888809fbd89c5a17473552bc3daf61))

- Fix `read_file()` NULL termination off-by-one error ([`902de42`](https://github.com/TooTallNate/nx.js/commit/902de42dfc0e968bf2e549048552e0fd3c160980))

- Accept `BufferSource` in `Switch.read()` and string in `Switch.write()` ([`b3a0810`](https://github.com/TooTallNate/nx.js/commit/b3a0810d0482e781c00799f8e589a4c6958f3686))

- Add Canvas `miterLimit` ([`02722a5`](https://github.com/TooTallNate/nx.js/commit/02722a553dcd900eb2822b13b96630e502540d8f))

## 0.0.10

### Patch Changes

- 6074fb2: Add web streams polyfill
- 835efcf: Fix infinite loop upon circular references in `Switch.inspect()`
- 150e55c: Add `Switch.chdir()`
- 209ebb8: Fix indexing in `Switch.argv`
- 01befe9: Fix `Switch.readDirSync()` throwing an error upon failure
- b250662: Set NACP version for output `nxjs.nro`
- 806737b: Add `Switch.stat()`
- b0dde04: Fix memory leak in `Switch.env.toObject()`
- 168a024: Add `Promise` introspection to `Switch.inspect()`
- 95e5954: Add `Switch.remove()`
- 26d38f0: Add `Switch.env.delete()` and add error handling for env `get()` and `set()`
- 091683b: Add class name when inspecting objects in `Switch.inspect()`
- 97afca5: Add `Switch.version` object
- ce8f8ac: Fix resolving more than one IP address in `Switch.resolveDns()`
- 901d941: Add `AbortController` and `AbortSignal` polyfills

## 0.0.9

### Patch Changes

- fd5b863: Disable err printing on thpool
  (for some reason, this fixes printing without a newline, for example on the REPL app)
- 19e11c7: Add threadpool, with new asynchronous functions:
  - `Switch.readFile() -> Promise<ArrayBuffer>`
  - `Switch.resolveDns() -> Promise<string[]>`
- b050eb6: Fix segfault upon exit when rendering in "console" mode

## 0.0.8

### Patch Changes

- c553611: Properly initialize FontFace class ID
- 82452c4: Add `console.error()`
- ab44617: Add `Switch.entrypoint`
- 7de26ff: Render `undefined` as grey
- 3cb2a94: Add support for `Promise` fulfillment
- d3b658b: Add `console.warn()`
- 94a90f1: Add `Switch.inspect()`
- b22fd00: Add yellow coloring to `console.warn()`
- 81d325b: Add Error inspection and custom inspect symbol
- dcdeefb: Add `Switch.argv` TypeScript definition
- 91ec2ec: Add `Switch.native.appletGetAppletType()` and `AppletType` enum to constants package

## 0.0.7

### Patch Changes

- 02b388c: Make `console.log` be a bound function
- b084cf7: Fix backslash key mapping
- 9b52d78: Filter out "." and ".." from `Switch.readDirSync()`

## 0.0.6

### Patch Changes

- 0cc389c: NULL terminate the source code file reads
- 24e9dff: Add initial keyboard support with `keydown` and `keyup` events
- d0819bf: Remove test Class ID code

## 0.0.5

### Patch Changes

- 4bc3271: .

## 0.0.4

### Patch Changes

- b889930: Debugging release workflow

## 0.0.3

### Patch Changes

- a0867ca: Debug release workflow

## 0.0.2

### Patch Changes

- 343c3d7: Begin implementing `ctx.measureText()`
