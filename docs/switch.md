# `Switch`

The `Switch` global object contains native interfaces to interact with the Switch hardware.

## Properties

#### `Switch.argv` -> `string[]`

Array of the arguments passed to the process. Under normal circumstances, this array contains a single entry with the absolute path to the `.nro` file.

```ts
console.log(Switch.argv);
// [ "sdmc:/switch/nxjs.nro" ]
```

#### `Switch.entrypoint` -> `string`

String value of the entrypoint JavaScript file that was evaluated. If a `main.js` file is present on the application's RomFS, then that will be executed first, in which case the value will be `romfs:/main.js`. Otherwise, the value will be the path of the `.nro` file on the SD card, with the `.nro` extension replaced with `.js`.

```ts
// In RomFS mode
console.log(Switch.entrypoint);
// romfs:/main.js

// In SD card mode (assuming the `.nro` file is located at `sdmc:/switch/nxjs.nro`)
console.log(Switch.entrypoint);
// sdmc:/switch/nxjs.js
```

#### `Switch.screen` -> [`Canvas`](./canvas.md)

Resembles the HTML [`Canvas`](./canvas.md) object which can have draw operations performed on it.
The `width` and `height` properties are set to the Switch screen resolution.
Use `getContext('2d')` to get the [canvas rendering context](https://developer.mozilla.org/en-US/docs/Web/API/CanvasRenderingContext2D) instance. See the [canvas](../apps/canvas/) application for an example of drawing to the canvas context.

#### `Switch.fonts` -> [`FontFaceSet`](https://developer.mozilla.org/en-US/docs/Web/API/FontFaceSet)

Contains the available fonts for use on the screen Canvas context.
By default, there is only one font available, which is the system font provided by the Switch operating system.
See the [ttf-fonts](../apps/ttf-font/) application for an example of using custom fonts.

## Methods

#### `Switch.cwd()` -> `URL`

Returns the current working directory of the process, as a `URL` instance. Can be useful to
create other `URL` instances relative to the location of the app's `.nro` file.

```ts
console.log(Switch.cwd());
// smdc:/switch/example/

console.log(new URL('assets/logo.png', Switch.cwd());
// smdc:/switch/example/assets/logo.png
```

#### `Switch.resolveDns(hostname: string)` -> `string[]`

Performs a DNS lookup to resolve a hostname to an array of IP addresses.

#### `Switch.readDirSync(path: string | URL)` -> `string[]`

Returns an array of file names that exist within the directory `path`.

#### `Switch.readFile(path: string | URL)` -> `Promise<ArrayBuffer>`

Reads the file at path `path`.

#### `Switch.readFileSync(path: string | URL)` -> `ArrayBuffer`

Synchronously reads the file at path `path`.

## Events

The `Switch` global is also an `EventTarget` instance, and may emit the following events:

#### `frame` event

Emitted for every pass of the event loop (at around 60 frames per second).
The nx.js runtime uses this internally to process interactions like timers, button presses and touchscreen events.
Your application code may also use this event to update the UI or render a game frame at the highest frame rate.

```ts
let frameCount = 0;
Switch.addEventListener('frame', () => {
    console.log(`frame: ${frameCount++}`);
});
```

#### `exit` event

Emitted exactly once, immediately before the application will exit. This event happens after the event loop has ended,
so it is not possible to schedule any asynchronous work during this event. Can be used to clean up any resources
or to save some state to the filesystem (synchronously). Note that this event will _not_ be emitted if the app is exited
by pressing the Home button.

```ts
Switch.addEventListener('exit', () => {
    const logFile = new URL('debug.log', Switch.cwd());
    Switch.writeFileSync(logFile, `Exited at ${new Date()}`);
});
```

#### `buttondown` event

Emitted when any button on the controller has been pressed down since the previous frame.
The `event.detail` property describes all newly pressed buttons.
You may use `Button` enum from the `nxjs-constants` module with a bitwise `&` to check for a specific button.

```ts
import { Button } from 'nxjs-constants';

Switch.addEventListener('buttondown', (event) => {
    if (event.detail & Button.B) {
        console.log('Button "B" pressed');
    }
});
```

#### `buttonup` event

Emitted when any button on the controller has been released since the previous frame.
The `event.detail` property describes all buttons which have just been released.
You may use `Button` enum from the `nxjs-constants` module with a bitwise `&` to check for a specific button.

```ts
import { Button } from 'nxjs-constants';

Switch.addEventListener('buttonup', (event) => {
    if (event.detail & Button.B) {
        console.log('Button "B" released');
    }
});
```

#### `touchstart`, `touchmove`, `touchend` events

Emitted while interacting with the touchscreen. These events follow the Web [Touch events](https://developer.mozilla.org/en-US/docs/Web/API/Touch_events) standard. See the [touchscreen](../apps/touchscreen/) application for an example of utilizing touch events.

```ts
Switch.addEventListener('touchmove', (e) => {
    for (const touch of e.changedTouches) {
        console.log(`${touch.identifier}: ${touch.screenX} x ${touch.screenY}`);
    }
});
```
