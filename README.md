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
-   **Graphics and UI**: Create visually appealing and interactive user interfaces using the web [`Canvas`](https://developer.mozilla.org/docs/Web/API/Canvas_API) API.
-   **Audio Support**: Integrate audio playback and sound effects into your applications using the web [`Audio`](https://developer.mozilla.org/docs/Web/API/HTMLAudioElement/Audio) API.
-   **WebAssembly**: Support for executing code compiled to [WebAssembly (WASM)](https://developer.mozilla.org/docs/WebAssembly).

## Getting Started

1. Set up a custom firmware on your Switch, so that it can run homebrew applications ([instructions](https://nh-server.github.io/switch-guide/)).
1. Download the `nxjs.nro` file from the [Releases](https://github.com/TooTallNate/nx.js/releases) page.
1. Create a file called `nxjs.js` with the following contents:
    ```javascript
    console.log('Hello Switch, from JavaScript!');
    ```
1. Copy both files into the `/switch` directory on your SD card.
1. Launch the app from the homebrew loader.
1. Profit!

See the [API docs](https://nxjs.n8.io) for further details, and check out the [`apps`](./apps) directory for examples.

## Creating an application

Run the following command to bootstrap the creation of an nx.js application:

```bash
npm create nxjs-app@latest
```

You will be able to choose from one of the example applications as a starting point.
Follow the prompts, and afterwards a new directory will be created with the project
name that you entered.

The following `package.json` scripts are configured:

 * `build` - Bundle the application code into a single JavaScript file using [`esbuild`](https://esbuild.github.io)
 * `nro` - Package the bundled app (+ any other files in the `romfs` dir) into a self-contained `.nro` file

## Contributing

Contributions to nx.js are welcome! If you find any issues or have
suggestions for improvements, please open an issue or submit a
pull request in the [GitHub repository](https://github.com/TooTallNate/nx.js).

[**Join the Discord server!**](https://discord.gg/MMmn73nsGz)

## License

nx.js is released under the MIT License. Please see
the [`LICENSE`](./LICENSE) file for more details.

[QuickJS]: https://bellard.org/quickjs/
