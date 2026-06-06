---
"@nx.js/runtime": minor
---

feat: configurable on-screen `console` — `Console` now accepts terminal styling options (theme colors, `fontSize`, `lineHeight`, `scrollback`, `cursorStyle`, `cursorOpacity`) via its constructor, and the global `console` exposes a settable `console.options` to apply them before the first log (the terminal is created lazily). The console can also be themed **declaratively** via a `[console]` section in `nxjs.ini` (`font_size`, `cursor_style`, `background`/`foreground`/`cursor`, and the full `black`..`bright_white` ANSI palette), exposed on `$.config.console`; the global console seeds its options from it (an explicit `console.options =` assignment overrides). The canvas terminal renderer now honors the **full** xterm ANSI palette (`black`..`brightWhite`, not just `background`/`foreground`/`cursor`) and supports `block`/`underline`/`bar` cursor styles. Also fixes thin background seams between cells by snapping the monospace cell advance to a whole pixel.
