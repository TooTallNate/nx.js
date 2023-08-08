# nx.js

<img align="right" width="200" height="200" src="./assets/logo.png">

**nx.js** is a framework that enables the development of Nintendo Switch
homebrew applications using JavaScript. Powered by the [QuickJS][] engine,
nx.js provides a streamlined environment for writing homebrew applications
for the Nintendo Switch console.

With nx.js, developers can leverage their JavaScript skills and tools to
create engaging and interactive experiences for the Nintendo Switch platform.
The framework abstracts the underlying low-level details and provides a
high-level JavaScript API that simplifies the development process.

nx.js is designed with Web standards in mind, so familiar APIs like
`setTimeout()`, `fetch()`, `new URL()`, `Canvas` and much more are
supported. If you are familar with web development then you should feel
right at home.

## Features

-   **JavaScript Development**: Write homebrew applications for the Nintendo Switch using JavaScript, a popular and widely supported programming language.
-   **High-Level API**: Benefit from a high-level JavaScript API designed specifically for the Nintendo Switch platform, providing easy access to console-specific features and functionality.
-   **Input Handling**: Capture and process user input with ease, including buttons, touch screen, and motion controls, to create engaging gameplay experiences.
-   **Graphics and UI**: Create visually appealing and interactive user interfaces using the web [`Canvas`](https://developer.mozilla.org/en-US/docs/Web/API/Canvas_API) API.
-   **Audio Support**: Integrate audio playback and sound effects into your applications using the web [`Audio`](https://developer.mozilla.org/en-US/docs/Web/API/HTMLAudioElement/Audio) API.
-   **No Compilation Necessary**: Just drop a `.js` file with the name matching the nx.js `.nro` file onto your SD card.

## Getting Started

1. Download the `nxjs.nro` file from the Releases page.
1. Create a file called `nxjs.js` with the following contents:
    ```javascript
    console.log('Hello Switch, from JavaScript!');
    ```
1. Copy both files into the `/switch` directory on your SD card.
1. Launch the app from the homebrew loader.
1. Profit!

See the [API docs](./docs/globals.md) for further details, and check out the [`apps`](./apps) directory for examples.

## Creating an application

To bootstrap the creation of an nx.js application, run the following command:

```bash
npm create nxjs-app@latest
```

You will be able to choose from one of the example applications as a starting point.
Follow the prompts, and afterwards a new directory will be created with the project
name that you entered.

The project will be configured to bundle the application code with `esbuild`.

## Contributing

Contributions to nx.js are welcome! If you find any issues or have
suggestions for improvements, please open an issue or submit a
pull request in the [GitHub repository](https://github.com/TooTallNate/nx.js).

## License

nx.js is released under the MIT License. Please see
the [`LICENSE`](./LICENSE) file for more details.

[QuickJS]: https://bellard.org/quickjs/
