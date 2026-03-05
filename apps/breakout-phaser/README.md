# Breakout (Phaser 3) — nx.js

A proof-of-concept that runs a **Phaser 3** game on the Nintendo Switch via [nx.js](https://github.com/nicknisi/nx.js).

## Why this matters

Phaser is the most popular HTML5 game framework, but it assumes a full browser DOM.
nx.js provides only a bare `screen` canvas, `OffscreenCanvas`, and basic Web APIs — no `document`, no DOM tree, no CSS.

This app bridges the gap with a **DOM shim** (`src/dom-shim.ts`) that stubs just enough of the DOM surface for Phaser's Canvas renderer to initialise and run.
If this approach works, it opens the door to porting the huge library of existing Phaser games to the Switch.

## Architecture

| File | Purpose |
|------|---------|
| `src/dom-shim.ts` | Minimal DOM shim — `document`, `HTMLCanvasElement`, `window.*` stubs |
| `src/input-adapter.ts` | Maps Switch gamepad D-pad → keyboard events, touch → pointer events |
| `src/main.ts` | Phaser game config + Breakout scene (procedural textures, arcade physics) |
| `src/fps.ts` | FPS counter utility |

## Controls

- **D-pad left / right** — move paddle
- **A button** — launch ball

## Building

```bash
cd apps/breakout-phaser
pnpm build   # esbuild → romfs/main.js
pnpm nro      # package as .nro
```

## Based on

- [MDN: 2D Breakout game with Phaser](https://developer.mozilla.org/en-US/docs/Games/Tutorials/2D_breakout_game_Phaser)
- Phaser 3 Arcade Physics
