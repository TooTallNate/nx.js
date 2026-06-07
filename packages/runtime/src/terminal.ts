import { Terminal as XTerminal } from '@xterm/headless';
import { OffscreenCanvas } from './canvas/offscreen-canvas';
import type { OffscreenCanvasRenderingContext2D } from './canvas/offscreen-canvas-rendering-context-2d';
import { fonts } from './font/font-face-set';
import { FontFace } from './font/font-face';
import { readFileSync } from './fs';

// Default ANSI 16-color palette (indices 0-15). A theme may override any of
// these via its `black`..`brightWhite` fields (see THEME_ANSI_KEYS).
const ANSI_COLORS = [
	'#000000', '#cd0000', '#00cd00', '#cdcd00',
	'#0000ee', '#cd00cd', '#00cdcd', '#e5e5e5',
	'#7f7f7f', '#ff0000', '#00ff00', '#ffff00',
	'#5c5cff', '#ff00ff', '#00ffff', '#ffffff',
];

// xterm ITheme field names for ANSI palette indices 0-15, in order. Used to
// resolve a theme's per-color overrides over the ANSI_COLORS defaults.
const THEME_ANSI_KEYS: (keyof ConsoleTheme)[] = [
	'black', 'red', 'green', 'yellow',
	'blue', 'magenta', 'cyan', 'white',
	'brightBlack', 'brightRed', 'brightGreen', 'brightYellow',
	'brightBlue', 'brightMagenta', 'brightCyan', 'brightWhite',
];

// Default screen size (matches `screen`).
const SCREEN_WIDTH = 1280;
const SCREEN_HEIGHT = 720;

// Geist Mono is shipped in the nx.js NRO's own RomFS, mounted as `nxjs:`.
const FONT_FAMILY = 'Geist Mono';
const FONT_PATH = 'nxjs:/GeistMono.ttf';

// undefined = not yet attempted; true/false = cached result.
let fontAvailable: boolean | undefined;
/**
 * Register the bundled Geist Mono font (from the nx.js NRO's own RomFS) into the
 * canvas font set. Returns false when the font can't be loaded — which is the
 * signal that the canvas-backed console is unavailable (e.g. the host test
 * binary, which has no `nxjs:` mount and no display), so callers fall back to
 * the native libnx console (`$.print`). The result is cached (incl. failure) so
 * the per-log check is cheap and doesn't re-`readFileSync` on the host.
 */
export function consoleFontAvailable(): boolean {
	if (fontAvailable !== undefined) return fontAvailable;
	try {
		const buf = readFileSync(FONT_PATH);
		if (buf) {
			fonts.add(new FontFace(FONT_FAMILY, buf));
			fontAvailable = true;
			return true;
		}
	} catch {
		/* fall through */
	}
	fontAvailable = false;
	return false;
}

/** Visual style of the terminal cursor. */
export type CursorStyle = 'block' | 'underline' | 'bar';

/**
 * Color theme for the on-screen terminal. Supports the standard ANSI 16-color
 * palette plus foreground, background, and cursor colors.
 */
export interface ConsoleTheme {
	/** The default foreground color. */
	foreground?: string;
	/** The default background color. */
	background?: string;
	/** The cursor color. */
	cursor?: string;
	/** The accent color of the cursor (fg color for a block cursor). */
	cursorAccent?: string;
	/** The selection background color (can be transparent). */
	selection?: string;
	/** ANSI black (eg. `\x1b[30m`). */
	black?: string;
	/** ANSI red (eg. `\x1b[31m`). */
	red?: string;
	/** ANSI green (eg. `\x1b[32m`). */
	green?: string;
	/** ANSI yellow (eg. `\x1b[33m`). */
	yellow?: string;
	/** ANSI blue (eg. `\x1b[34m`). */
	blue?: string;
	/** ANSI magenta (eg. `\x1b[35m`). */
	magenta?: string;
	/** ANSI cyan (eg. `\x1b[36m`). */
	cyan?: string;
	/** ANSI white (eg. `\x1b[37m`). */
	white?: string;
	/** ANSI bright black (eg. `\x1b[1;30m`). */
	brightBlack?: string;
	/** ANSI bright red (eg. `\x1b[1;31m`). */
	brightRed?: string;
	/** ANSI bright green (eg. `\x1b[1;32m`). */
	brightGreen?: string;
	/** ANSI bright yellow (eg. `\x1b[1;33m`). */
	brightYellow?: string;
	/** ANSI bright blue (eg. `\x1b[1;34m`). */
	brightBlue?: string;
	/** ANSI bright magenta (eg. `\x1b[1;35m`). */
	brightMagenta?: string;
	/** ANSI bright cyan (eg. `\x1b[1;36m`). */
	brightCyan?: string;
	/** ANSI bright white (eg. `\x1b[1;37m`). */
	brightWhite?: string;
	/** ANSI extended colors (16-255). */
	extendedAnsi?: string[];
}

export interface TerminalOptions {
	/** Width of the backing canvas in pixels. @default 1280 */
	width?: number;
	/** Height of the backing canvas in pixels. @default 720 */
	height?: number;
	/** Font size in pixels. @default 20 */
	fontSize?: number;
	/**
	 * Line height as a multiple of the font size. @default 1.25
	 */
	lineHeight?: number;
	/** Scrollback buffer size (rows). @default 1000 */
	scrollback?: number;
	/**
	 * Color theme. In addition to `background` / `foreground` / `cursor`, the
	 * full ANSI palette (`black`..`brightWhite`) is honored.
	 */
	theme?: ConsoleTheme;
	/** Cursor shape. @default 'block' */
	cursorStyle?: CursorStyle;
	/**
	 * Opacity of the cursor, 0-1. @default 0.5
	 */
	cursorOpacity?: number;
}

/**
 * A canvas-backed terminal. Wraps a headless xterm.js instance for terminal
 * state/scrollback/ANSI parsing, and renders the cell grid into an
 * {@link OffscreenCanvas} using the Canvas 2D API with the bundled Geist Mono
 * font. Feed it ANSI text with {@link Terminal.write | `write()`}; the rendered
 * canvas is available as {@link Terminal.canvas | `canvas`}.
 */
export class Terminal {
	#term: XTerminal;
	#canvas: OffscreenCanvas;
	#ctx: OffscreenCanvasRenderingContext2D;
	#fontSize: number;
	#charWidth: number;
	#lineHeight: number;
	#theme: ConsoleTheme;
	/** Resolved 16-color ANSI palette (theme overrides over the defaults). */
	#palette: string[];
	#cursorStyle: CursorStyle;
	#cursorOpacity: number;
	#scrollOffset = 0;
	/** Set whenever the buffer/cursor/scroll changes; cleared after render(). */
	#dirty = true;

	constructor(opts: TerminalOptions = {}) {
		const width = opts.width ?? SCREEN_WIDTH;
		const height = opts.height ?? SCREEN_HEIGHT;
		// Sanitize numeric options: a bad value (0, negative, NaN, Infinity)
		// from the JS API or nxjs.ini must not break layout (e.g. a 0/NaN line
		// height makes `rows` Infinity/NaN) or rendering (globalAlpha ignores
		// out-of-range assignments). Fall back to the default for each.
		const posOr = (v: number | undefined, dflt: number) =>
			typeof v === 'number' && isFinite(v) && v > 0 ? v : dflt;
		this.#fontSize = posOr(opts.fontSize, 20);
		const lineHeightMul = posOr(opts.lineHeight, 1.25);
		this.#theme = opts.theme ?? {};
		this.#cursorStyle = opts.cursorStyle ?? 'block';
		// Clamp cursor opacity to [0, 1]; a non-finite value falls back to 0.5.
		const co = opts.cursorOpacity;
		this.#cursorOpacity =
			typeof co === 'number' && isFinite(co)
				? Math.max(0, Math.min(1, co))
				: 0.5;

		// Resolve the ANSI palette: theme color overrides each default.
		this.#palette = ANSI_COLORS.map(
			(def, i) => (this.#theme[THEME_ANSI_KEYS[i]!] as string) ?? def,
		);

		consoleFontAvailable();

		this.#canvas = new OffscreenCanvas(width, height);
		this.#ctx = this.#canvas.getContext('2d')!;

		// Measure the monospace advance with the real font so cols/rows match
		// the chosen glyph metrics (fall back to a 0.6em estimate).
		this.#ctx.font = `${this.#fontSize}px "${FONT_FAMILY}"`;
		let charWidth = 0;
		try {
			charWidth = this.#ctx.measureText('M').width;
		} catch {
			/* ignore */
		}
		if (!charWidth || !isFinite(charWidth)) {
			charWidth = this.#fontSize * 0.6;
		}
		// Snap the cell advance to a whole pixel. With a fractional advance,
		// per-cell rounding (round(x*cw)..round((x+1)*cw)) gives cells that
		// alternate between floor/ceil widths; a glyph (e.g. the FULL BLOCK
		// `█` used for color swatches) drawn at its own fractional advance then
		// leaves thin background seams between cells. An integer advance makes
		// every cell exactly `cw` px wide so glyphs and fills tile seamlessly.
		this.#charWidth = Math.max(1, Math.round(charWidth));
		this.#lineHeight = Math.max(1, Math.ceil(this.#fontSize * lineHeightMul));

		const cols = Math.max(1, Math.floor(width / this.#charWidth));
		const rows = Math.max(1, Math.floor(height / this.#lineHeight));

		this.#term = new XTerminal({
			cols,
			rows,
			scrollback: opts.scrollback ?? 1000,
			allowProposedApi: true,
		});

		const markDirty = () => {
			this.#dirty = true;
		};
		this.#term.onCursorMove(markDirty);
		this.#term.onLineFeed(markDirty);
		this.#term.onScroll(markDirty);
		this.#term.onWriteParsed(markDirty);
	}

	/** The {@link OffscreenCanvas} the terminal renders into. */
	get canvas(): OffscreenCanvas {
		return this.#canvas;
	}

	/**
	 * The underlying headless
	 * {@link https://xtermjs.org/docs/api/terminal/classes/terminal/ | xterm.js Terminal}
	 * instance, typed as `unknown` to avoid leaking the `@xterm/headless`
	 * dependency into the public type surface. Cast to
	 * `import('@xterm/headless').Terminal` if you need the full API.
	 */
	get terminal(): unknown {
		return this.#term;
	}

	/** Whether the canvas needs to be re-rendered + re-presented. */
	get dirty(): boolean {
		return this.#dirty;
	}

	/** Number of rows scrolled back from the bottom (0 = latest). */
	get scrollOffset(): number {
		return this.#scrollOffset;
	}
	set scrollOffset(v: number) {
		const clamped = Math.max(0, Math.min(v, this.#term.buffer.active.baseY));
		if (clamped !== this.#scrollOffset) {
			this.#scrollOffset = clamped;
			this.#dirty = true;
		}
	}

	/** Write ANSI text to the terminal. Resets scrollback to the latest output. */
	write(data: string): void {
		if (this.#scrollOffset !== 0) {
			this.#scrollOffset = 0;
		}
		this.#dirty = true;
		// A terminal treats LF as "move down one row" only — the cursor keeps
		// its column, so a bare "\n" (as produced by console.log et al.)
		// staircases. Normalize lone LF to CRLF (carriage return + line feed) so
		// each new line starts at column 0, without doubling an existing CR.
		this.#term.write(data.replace(/\r?\n/g, '\r\n'));
	}

	/** Scroll the viewport up `n` rows into the scrollback history. */
	scrollUp(n = 1): void {
		this.scrollOffset = this.#scrollOffset + n;
	}
	/** Scroll the viewport down `n` rows toward the latest output. */
	scrollDown(n = 1): void {
		this.scrollOffset = this.#scrollOffset - n;
	}
	/** Scroll all the way up to the start of the scrollback history. */
	scrollToTop(): void {
		this.scrollOffset = this.#term.buffer.active.baseY;
	}
	/** Scroll back down to the latest output. */
	scrollToBottom(): void {
		this.scrollOffset = 0;
	}

	#cellColor(
		colorCode: number,
		isBold: boolean,
		isDefault: boolean,
		isRGB: boolean,
		fallback: string,
	): string {
		const fg = fallback;
		if (isDefault) return fg;
		// 24-bit truecolor (e.g. kleur's rgb()/bgRgb()): getColor() returns the
		// packed 0xRRGGBB value. Must be handled BEFORE the palette ranges,
		// since a packed RGB int is almost always > 255.
		if (isRGB) {
			const r = (colorCode >> 16) & 0xff;
			const g = (colorCode >> 8) & 0xff;
			const b = colorCode & 0xff;
			return `rgb(${r},${g},${b})`;
		}
		if (colorCode === -1) return fg;
		if (colorCode >= 0 && colorCode <= 15) {
			if (isBold && colorCode < 8) return this.#palette[colorCode + 8]!;
			return this.#palette[colorCode]!;
		}
		if (colorCode >= 16 && colorCode <= 231) {
			const c = colorCode - 16;
			const r = Math.floor(c / 36) * 51;
			const g = (Math.floor(c / 6) % 6) * 51;
			const b = (c % 6) * 51;
			return `rgb(${r},${g},${b})`;
		}
		if (colorCode >= 232 && colorCode <= 255) {
			const v = (colorCode - 232) * 10 + 8;
			return `rgb(${v},${v},${v})`;
		}
		return fg;
	}

	/**
	 * Re-render the terminal buffer into the backing canvas. No-op when not
	 * dirty (cheap to call every frame). Returns whether a render happened.
	 */
	render(): boolean {
		if (!this.#dirty) return false;
		this.#dirty = false;

		const ctx = this.#ctx;
		const buff = this.#term.buffer.active;
		const cell = buff.getNullCell();
		const w = this.#canvas.width;
		const h = this.#canvas.height;
		const cw = this.#charWidth;
		const lh = this.#lineHeight;
		const rows = this.#term.rows;

		const maxOffset = buff.baseY;
		const offset = Math.max(0, Math.min(this.#scrollOffset, maxOffset));
		this.#scrollOffset = offset;
		const startRow = buff.viewportY - offset;

		// Background.
		ctx.fillStyle = this.#theme.background ?? '#000000';
		ctx.fillRect(0, 0, w, h);

		ctx.textBaseline = 'top';
		const baseFont = `${this.#fontSize}px "${FONT_FAMILY}"`;
		const boldFont = `bold ${this.#fontSize}px "${FONT_FAMILY}"`;

		const bg = this.#theme.background ?? '#000000';
		const fg = this.#theme.foreground ?? '#ffffff';
		for (let y = 0; y < rows; y++) {
			const line = buff.getLine(startRow + y);
			if (!line) continue;
			// Integer pixel bounds for this row, so adjacent rows share an exact
			// edge with no sub-pixel seam.
			const yTop = Math.round(y * lh);
			const yBot = Math.round((y + 1) * lh);
			for (let x = 0; x < line.length; x++) {
				line.getCell(x, cell);
				const char = cell.getChars();

				// Integer pixel bounds for this cell. Using rounded left/right
				// edges (rather than left + fractional width) makes neighboring
				// cell backgrounds tile seamlessly — no 1px black gaps.
				const xL = Math.round(x * cw);
				const xR = Math.round((x + 1) * cw);

				// Cell background (skip default — already cleared).
				if (!cell.isBgDefault()) {
					ctx.fillStyle = this.#cellColor(
						cell.getBgColor(),
						false,
						false,
						cell.isBgRGB(),
						bg,
					);
					ctx.fillRect(xL, yTop, xR - xL, yBot - yTop);
				}

				if (!char) continue;
				const bold = cell.isBold() !== 0;
				ctx.fillStyle = this.#cellColor(
					cell.getFgColor(),
					bold,
					cell.isFgDefault(),
					cell.isFgRGB(),
					fg,
				);
				ctx.font = bold ? boldFont : baseFont;
				ctx.fillText(char, xL, yTop);
			}
		}

		// Cursor (only when viewing the latest output).
		if (offset === 0 && this.#cursorOpacity > 0) {
			ctx.fillStyle = this.#theme.cursor ?? '#ffffff';
			ctx.globalAlpha = this.#cursorOpacity;
			const cxL = Math.round(buff.cursorX * cw);
			const cxR = Math.round((buff.cursorX + 1) * cw);
			const cyT = Math.round(buff.cursorY * lh);
			const cyB = Math.round((buff.cursorY + 1) * lh);
			const cw_ = cxR - cxL;
			const ch_ = cyB - cyT;
			if (this.#cursorStyle === 'bar') {
				// Thin vertical bar at the left edge (min 1px).
				ctx.fillRect(cxL, cyT, Math.max(1, Math.round(cw_ * 0.15)), ch_);
			} else if (this.#cursorStyle === 'underline') {
				// Thin horizontal bar at the bottom (min 1px).
				const uh = Math.max(1, Math.round(ch_ * 0.15));
				ctx.fillRect(cxL, cyB - uh, cw_, uh);
			} else {
				// Block (default): full cell.
				ctx.fillRect(cxL, cyT, cw_, ch_);
			}
			ctx.globalAlpha = 1;
		}
		return true;
	}
}
