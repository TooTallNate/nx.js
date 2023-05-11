# nx.js

**nx.js** is a framework that enables the development of Nintendo Switch
homebrew applications using JavaScript. Powered by the [QuickJS][] engine,
nx.js provides a streamlined environment for writing homebrew applications
on the Nintendo Switch console.

With nx.js, developers can leverage their JavaScript skills and tools to
create engaging and interactive experiences for the Nintendo Switch platform.
The framework abstracts the underlying low-level details and provides a
high-level JavaScript API that simplifies the development process.

## Features

-   **JavaScript Development**: Write homebrew applications for the Nintendo Switch using JavaScript, a popular and widely supported programming language.
-   **High-Level API**: Benefit from a high-level JavaScript API designed specifically for the Nintendo Switch platform, providing easy access to console-specific features and functionality.
-   **Resource Management**: Utilize convenient resource management mechanisms for loading and handling game assets such as graphics, audio, and data files.
-   **Input Handling**: Capture and process user input with ease, including buttons, touch screen, and motion controls, to create engaging gameplay experiences.
-   **Graphics and UI**: Create visually appealing and interactive user interfaces using the web [`Canvas`](https://developer.mozilla.org/en-US/docs/Web/API/Canvas_API) API.
-   **Audio Support**: Integrate audio playback and sound effects into your applications using the provided web [`Audio`](https://developer.mozilla.org/en-US/docs/Web/API/HTMLAudioElement/Audio) API.
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

See the [API docs](./api.md) for further details, and check out the [`apps`](./apps) directory for examples.

## Contributing

Contributions to nx.js are welcome! If you find any issues or have
suggestions for improvements, please open an issue or submit a
pull request in the [GitHub repository](https://github.com/TooTallNate/nx.js).

## License

nx.js is released under the MIT License. Please see
the [`LICENSE`](./LICENSE) file for more details.

[QuickJS]: https://bellard.org/quickjs/
