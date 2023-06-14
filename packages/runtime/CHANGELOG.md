# nxjs-runtime

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
