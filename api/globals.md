# Globals

## Native

The main native interface to the Switch hardware is provided by the `Switch` global:

-   [`Switch`](./switch.md)

## ES2020

The QuickJS engine supports ES2020 features, so stuff like `Promise`, `Map`, `Set`, `async` functions, etc. are supported.
For any non-supported features, it is recommended to polyfill those features by configuring a bundler, such as `esbuild`.

## Web Standards

Listed below are the officially supported Web interfaces:

-   [`console`](https://developer.mozilla.org/en-US/docs/Web/API/console)
    -   Only `console.log()` is implemented at this time, and will enable the console renderer when invoked.
-   [`setTimeout`](https://developer.mozilla.org/en-US/docs/Web/API/setTimeout)
-   [`setInterval`](https://developer.mozilla.org/en-US/docs/Web/API/setInterval)
-   [`clearTimeout`](https://developer.mozilla.org/en-US/docs/Web/API/clearTimeout)
-   [`clearInterval`](https://developer.mozilla.org/en-US/docs/Web/API/clearInterval)
-   [`FontFace`](https://developer.mozilla.org/en-US/docs/Web/API/FontFace)
    -   See the [`Switch.fonts`](./switch.md) property for more details.
-   [`EventTarget`](https://developer.mozilla.org/en-US/docs/Web/API/EventTarget)
-   [`URL`](https://developer.mozilla.org/en-US/docs/Web/API/URL)
-   [`URLSearchParams`](https://developer.mozilla.org/en-US/docs/Web/API/URLSearchParams)
-   [`Event`](https://developer.mozilla.org/en-US/docs/Web/API/Event)
    -   [`UIEvent`](https://developer.mozilla.org/en-US/docs/Web/API/UIEvent)
    -   [`TouchEvent`](https://developer.mozilla.org/en-US/docs/Web/API/TouchEvent)
-   [`TextDecoder`](https://developer.mozilla.org/en-US/docs/Web/API/TouchEvent)
    -   Only `utf-8` decoding is built-in. Use an npm module if you need a different encoding.
