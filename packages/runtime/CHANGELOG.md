# nxjs-runtime

## 0.0.21

### Patch Changes

- Add "utf8" as an accepted encoding for `TextDecoder` ([`b433831`](https://github.com/TooTallNate/nx.js/commit/b433831d7ad5491ebdfeff3ecc67b3f778c4d0da))

- Add `console.trace()` ([`cc6a443`](https://github.com/TooTallNate/nx.js/commit/cc6a443ff4b1e89414f098acda05dba5d6df31dc))

- Add `navigator.getBattery()` API ([`4d8f380`](https://github.com/TooTallNate/nx.js/commit/4d8f380357e2ce402b39ae8833d52d48fe6a7a0a))

- Make `globalThis` inherit from `EventTarget` ([`7f7d961`](https://github.com/TooTallNate/nx.js/commit/7f7d9610d74446b9af924ce05d0cb904891a53a5))

- Implement `WebAssembly.Memory#grow()` ([`ec79b19`](https://github.com/TooTallNate/nx.js/commit/ec79b199adb683d5f36a80b8c602dbe5b90176aa))

- Add "unhandledrejection" global event when a Promise is not caught ([`4eff094`](https://github.com/TooTallNate/nx.js/commit/4eff094b21fb48fa097ef5606e0902ded8d04f43))

- Fix reference to `BufferSource` type in WASM namespace ([`7541564`](https://github.com/TooTallNate/nx.js/commit/754156410b500159714c445e908b9bd2b9c72942))

- Add `navigator.maxTouchPoints` ([`2ef1bf8`](https://github.com/TooTallNate/nx.js/commit/2ef1bf8126be35610719cb0ecff9243a57c26e14))

- Add a few `EventTarget` related types / interfaces ([`a3b7ab6`](https://github.com/TooTallNate/nx.js/commit/a3b7ab664aad054a7081397c7df1a3a349d27fc0))

- Implement `import.meta` ([`612a208`](https://github.com/TooTallNate/nx.js/commit/612a2080f441045a728c4f109ce68312d78691ec))

- Free the arguments for thread pool callbacks ([`ddcc4b9`](https://github.com/TooTallNate/nx.js/commit/ddcc4b9ac093be4750b89d208c63019fe753b85c))

- Add type definitions for `EventTarget` interface on `globalThis` ([`e0f2439`](https://github.com/TooTallNate/nx.js/commit/e0f243931a75cdc9a778995a3d099a440e66da11))

- Continue event loop when `event.preventDefault()` is called for "error" or "unhandledrejection" ([`99ccc71`](https://github.com/TooTallNate/nx.js/commit/99ccc713db7f86ae99f6e3adc512df4f8a6b417f))

- Add source map tracing in error stack traces ([#48](https://github.com/TooTallNate/nx.js/pull/48))

- Add `ErrorEventInit` interface ([`99cf6dd`](https://github.com/TooTallNate/nx.js/commit/99cf6dd2989ea010d6763ebddaeb83310572c4ad))

- Add "error" global event when an Error is not caught ([`436f4d2`](https://github.com/TooTallNate/nx.js/commit/436f4d26cf5c93921738c99db19adb5172a49382))

- Implement initial `WebAssembly.Table` export functionality ([`9ef02b1`](https://github.com/TooTallNate/nx.js/commit/9ef02b117eac69b8563c0ac0877a52c6e639e2ed))

- Implement WASM imported function return value ([`a4fb327`](https://github.com/TooTallNate/nx.js/commit/a4fb32771f11eba2f00f1060626c18e9547ff889))

## 0.0.20

### Patch Changes

- Handle `WebAssembly.Memory` buffer growing ([`b950962`](https://github.com/TooTallNate/nx.js/commit/b95096252ac58ef69bf26ec05c492731fc840456))

- Delete the `leaks.txt` file when empty ([`11288bf`](https://github.com/TooTallNate/nx.js/commit/11288bfbc896042e9dc8dcc1a50024fdb6f8f513))

- Fix string memory leak in `Switch.writeFileSync()` ([`aa87ace`](https://github.com/TooTallNate/nx.js/commit/aa87acede559dab6d7a75153521cfdee46d8d95a))

- Fix NULL segfault when WASM module contains functions / globals without a name ([`d433882`](https://github.com/TooTallNate/nx.js/commit/d4338827bb0d0dc1407a35c1226032c46fbc6efd))

- Write QuickJS leaks detection output to `leaks.txt` file ([`78e4790`](https://github.com/TooTallNate/nx.js/commit/78e4790f3a6782d2f3938e7f7316a4133528b733))

- Implement WASM "memory" type import ([`ba0479a`](https://github.com/TooTallNate/nx.js/commit/ba0479a597ac160e05951fb38f50fa74300df959))

- Use Promise introspection APIs from updated QuickJS fork ([`a7c23d7`](https://github.com/TooTallNate/nx.js/commit/a7c23d7d35b64becad2f4374c86314bcef99b2c5))

- Fix string memory leak in `Switch.readFileSync()` ([`d5e032e`](https://github.com/TooTallNate/nx.js/commit/d5e032ebee780344e16e000da0e4d34b22e97b38))

- Fix another crash-on-exit ref counting issue ([`254d799`](https://github.com/TooTallNate/nx.js/commit/254d799b230a86c6e05e505ab79949273d6c6c44))

- Fix object memory leak in `CanvasRenderingContext2D` ([`cf578d8`](https://github.com/TooTallNate/nx.js/commit/cf578d8efe6ca7ccbe3baec9b15d04d36f1558d6))

## 0.0.19

### Patch Changes

- Add `navigator` global with `userAgent` property ([`2331b1e`](https://github.com/TooTallNate/nx.js/commit/2331b1e5ae8e3f0b13cfa03bb65997de4061dc91))

- Add `Switch.version.wasm3` ([`8ad22c4`](https://github.com/TooTallNate/nx.js/commit/8ad22c470baf40db1099d306fa27af55f01b6329))

- Make `data:` scheme URLs work with `fetch()` ([`9a9dee4`](https://github.com/TooTallNate/nx.js/commit/9a9dee4e819e9c623606e65aa54661cbd665d1d5))

- Make the `console` functions handle printf formatters and multiple arguments ([`d22e5d2`](https://github.com/TooTallNate/nx.js/commit/d22e5d290107c0a200f1638e796fe57ac936a949))

- Make `fetch()` URL resolve relative to `Switch.entrypoint` ([`cee00d0`](https://github.com/TooTallNate/nx.js/commit/cee00d0959fc163acbd1d989c5d9abdb19467c05))

- Free JS references to make process exit cleanly ([`dfa97ae`](https://github.com/TooTallNate/nx.js/commit/dfa97ae8f87cf99680cae5f64661110b0e1de3e8))

- Fix setting `WebAssembly.Global` value before being "bound" to a WASM module ([`0e3264e`](https://github.com/TooTallNate/nx.js/commit/0e3264e6a2ac6aaae7f01a7edca93d58aa409229))

## 0.0.18

### Patch Changes

- Add `console.debug()` as an alias for `console.log()` ([`81c6692`](https://github.com/TooTallNate/nx.js/commit/81c669297868ca19e21287d4ffd6ba06443480a3))

- Add custom inspector for `URL` instances ([`b44ff24`](https://github.com/TooTallNate/nx.js/commit/b44ff2480a55d7f8a7424fd0ede60b82c8eef732))

- Add docs for global `crypto` instance ([`b0b02b7`](https://github.com/TooTallNate/nx.js/commit/b0b02b78ae5c705fb391f1b486cfad9f22561ced))

- Add `WebAssembly` (WASM) implementation ([#12](https://github.com/TooTallNate/nx.js/pull/12))

- Fix `setTimeout()` / `clearTimeout()` bundle defined name ([`05a0ad6`](https://github.com/TooTallNate/nx.js/commit/05a0ad65fe09e06c5f621335193a6f7692127561))

- Fix spacing when rendering Error stack in `inspect()` ([`7d5d94c`](https://github.com/TooTallNate/nx.js/commit/7d5d94c205fe1039a7664e443dadbd0cd058601a))

- Make `ReadableStream` work with `for await...of` loops ([`27cb966`](https://github.com/TooTallNate/nx.js/commit/27cb9661b232d0cfeb12745cc189a2662d57fab5))

- Fix `inspect()` when `constructor` is falsy ([`daae521`](https://github.com/TooTallNate/nx.js/commit/daae52108957062ff37d13524b72321a58331060))

- Make `Switch.cwd()` return a string ([`7acbfad`](https://github.com/TooTallNate/nx.js/commit/7acbfad091a8626771a3e98c1bb2e157133ea7ce))

## 0.0.17

### Patch Changes

- Mark `DOMPoint.fromPoint()` as static ([`2c31aea`](https://github.com/TooTallNate/nx.js/commit/2c31aea00700e7aff1785eac76017e4d720e2cc9))

- Add esbuild bundle script ([`1e20a42`](https://github.com/TooTallNate/nx.js/commit/1e20a42b4c8f971fb72594682e691aca2fc56450))

- Remove private class field usage from `Blob` ([`f1a4837`](https://github.com/TooTallNate/nx.js/commit/f1a483751f49cc1a766937f6e9e7c6efa2f372ca))

- Move a few Event-related interfaces in-house ([`93c5c59`](https://github.com/TooTallNate/nx.js/commit/93c5c59603fa09269de1f4f7afea74d392bd154d))

- Update `typescript` to v5.2.2 ([`7e03f10`](https://github.com/TooTallNate/nx.js/commit/7e03f10787a30087d40509fef563c1349bb9b860))

- More robust filtering of `globalThis` interfaces on compiled type definitions ([`07801fe`](https://github.com/TooTallNate/nx.js/commit/07801fe9dbd1d04ed6e97ac5f8fdfb3fb1a0c349))

- Add support for DOMPoint value for `radii` parameter in `Canvas#roundRect()` ([`deee32f`](https://github.com/TooTallNate/nx.js/commit/deee32fe4aa62beb10eb92f5d2f1ca8afb394066))

## 0.0.16

### Patch Changes

- Use TypeDoc to generate docs website ([`ef53cef`](https://github.com/TooTallNate/nx.js/commit/ef53cef1c57e66f0855df92977cc6f0d4a17de27))

- Rename `Switch` class to `SwitchClass` ([`ef758a6`](https://github.com/TooTallNate/nx.js/commit/ef758a69df898a12ceaab809e9bd5327cb8c2041))

- Add docs for `URLSearchParams` ([`4560eef`](https://github.com/TooTallNate/nx.js/commit/4560eef63b0edcc5ba40aeba5a0880e4ccc67287))

- Remove `INTERNAL_SYMBOL` from `CanvasRenderingContext2D` ([`583cfd4`](https://github.com/TooTallNate/nx.js/commit/583cfd458e615264ae11a5a3a84f6c8450c2c38d))

- Remove instances of `globalThis` from types ([`62ceed4`](https://github.com/TooTallNate/nx.js/commit/62ceed41402bcddfc2216c2782a702789f3204f9))

- Disallow "body" for `GET`/`HEAD` requests ([`e60d92a`](https://github.com/TooTallNate/nx.js/commit/e60d92affebcb7a819d5f32c27aeabc52fc7b0b1))

- Add `Switch.vibrate()` ([`e742556`](https://github.com/TooTallNate/nx.js/commit/e742556c9110f786be80a2b756cff7b1c32f4506))

- Add "png", "turbojpeg" and "webp" to `Switch.version` ([`6596d91`](https://github.com/TooTallNate/nx.js/commit/6596d919f7ef8dd97f06cb1cb9ab48a4aed0bc32))

- Remove "es2020.intl" since `Intl` is not implemented in QuickJS ([`69f38a2`](https://github.com/TooTallNate/nx.js/commit/69f38a24f1b1bc19d60361a125992b16f7cd27e3))

- Begin moving DOM interfaces in-house ([`c45b11b`](https://github.com/TooTallNate/nx.js/commit/c45b11b172555b0970b6d2b2cd9efb1361e2d904))

- Add `DOMPoint` and `DOMPointReadOnly` ([`8919b14`](https://github.com/TooTallNate/nx.js/commit/8919b14e5b8c41309cc131b371cc98cec1a95424))

- Clean up some libnx services upon exit ([`3cfb62a`](https://github.com/TooTallNate/nx.js/commit/3cfb62a3894afccf15b24436725ed53c0e126c17))

- Add docs for `EventTarget` ([`6dd003d`](https://github.com/TooTallNate/nx.js/commit/6dd003d15eaac45daec17f393b52d7e3ad5ba783))

- Use `dts-bundle-generator` to create runtime type definitions ([`4da0301`](https://github.com/TooTallNate/nx.js/commit/4da0301296828119305d5c40539514901d2f9fee))

- Add types for all Web Streams classes/interfaces ([`6f050e8`](https://github.com/TooTallNate/nx.js/commit/6f050e8560c628afdb565fa60347fc885f4b9156))

- Fix buffer size compile warning ([`0860f46`](https://github.com/TooTallNate/nx.js/commit/0860f463e392ab34a66a8006fc035795a1903e69))

- Add `Headers#getSetCookie()` ([`60ac070`](https://github.com/TooTallNate/nx.js/commit/60ac070d109aa89fb00ff800cf1f424d2614e1aa))

## 0.0.15

## 0.0.14

### Patch Changes

- Add `File` ([`192b1d1`](https://github.com/TooTallNate/nx.js/commit/192b1d13ea61c8a062003a95a06739cb795d4d3c))

- Add `FormData` ([`f21f042`](https://github.com/TooTallNate/nx.js/commit/f21f042c18032bc5d23540d4fd7fbe9ce2fd2e6c))

- Remove `Symbol.hasInstance` from `Blob` ([`6f5ef58`](https://github.com/TooTallNate/nx.js/commit/6f5ef58c77bfa9a456c690fcc708253a1e1c36d3))

- Fix `CanvasRenderingContext2D` class to be compatible with global type definition ([`019adac`](https://github.com/TooTallNate/nx.js/commit/019adac50c66e269de1319d68d68f3a185769f7d))

- Updated logo with vector ([#20](https://github.com/TooTallNate/nx.js/pull/20))

- Set `Symbol.toStringTag` on all global classes ([`e888b51`](https://github.com/TooTallNate/nx.js/commit/e888b51fbb83f13cd1fba03f4459e9b775037e5a))

- Add support for `blob:` URLs in `fetch()` ([`9f663e9`](https://github.com/TooTallNate/nx.js/commit/9f663e9a0e0ae1c20d095c324993e3fbdbce28f3))

- Add `ImageData` as a global ([`a2473e6`](https://github.com/TooTallNate/nx.js/commit/a2473e6d1db3d7b20a299089382562bed8de4d69))

- Fix Canvas `ctx.font` setter to not throw upon empty string ([`4ddb225`](https://github.com/TooTallNate/nx.js/commit/4ddb225c6280c1f7885f73120e8e1687a69ec41c))

- Add `URL.createObjectURL()` and `URL.revokeObjectURL()` ([`9905c8a`](https://github.com/TooTallNate/nx.js/commit/9905c8abbd1781fa9a3fffee650702aa9a5fe14f))

- Support `FormData` encode/decode in `Body` class ([`1df3bfe`](https://github.com/TooTallNate/nx.js/commit/1df3bfe09a3ea717071c80dae3a42c46caab022b))

- Fix passing `File` instance to `FormData` ([`ed38043`](https://github.com/TooTallNate/nx.js/commit/ed380430119d61285b41877bef4351295e0fe0f1))

- Support `path` parameter in Canvas `fill()` method ([`756b5cf`](https://github.com/TooTallNate/nx.js/commit/756b5cf98cc29e50ca4c554ac6ffd6d64e9d0039))

- Add `crypto.getRandomValues()` and `crypto.randomUUID()` ([`f4d007c`](https://github.com/TooTallNate/nx.js/commit/f4d007c15160c99caf020c7f5b49c93bf3a54fdf))

- Prompt for package manager (pnpm, npm or yarn) and install dependencies ([`a43c1ac`](https://github.com/TooTallNate/nx.js/commit/a43c1acae2d6b6c1a32f26ef6d88b0daa891dff3))

- Add `Path2D` ([`be90649`](https://github.com/TooTallNate/nx.js/commit/be90649022f7b7c56209cdcad338330dd618d2c9))

## 0.0.13

### Patch Changes

- Add `Image` (currently only PNG support) ([`0227d73`](https://github.com/TooTallNate/nx.js/commit/0227d73da26eaa65b38283e0c68282476617f648))

- Add Canvas `drawImage()` ([`9efc267`](https://github.com/TooTallNate/nx.js/commit/9efc267b4ebf3648ad199103665ea1ece4df73d6))

- Add support for WebP in `Image` ([`0fad48d`](https://github.com/TooTallNate/nx.js/commit/0fad48d0bb8acf2518972cc2aeb79b61e5c99d9d))

- Add Canvas `globalAlpha` ([`ccb10f5`](https://github.com/TooTallNate/nx.js/commit/ccb10f5da28440da6e59e2efbd42776875f70cfa))

- Add support for JPEG in `Image` ([`5fb65a4`](https://github.com/TooTallNate/nx.js/commit/5fb65a445beb33750eac0e68ecc84f981d226aed))

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
