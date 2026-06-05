import { Terminal as XTerminal, type ITheme } from '@xterm/headless';
import { OffscreenCanvas } from './canvas/offscreen-canvas';
import type { OffscreenCanvasRenderingContext2D } from './canvas/offscreen-canvas-rendering-context-2d';
import { fonts } from './font/font-face-set';
import { FontFace } from './font/font-face';
import { readFileSync } from './fs';

// ANSI 256-color palette (first 16 standard colors).
const ANSI_COLORS = [
	'#000000', '#cd0000', '#00cd00', '#cdcd00',
	'#0000ee', '#cd00cd', '#00cdcd', '#e5e5e5',
	'#7f7f7f', '#ff0000', '#00ff00', '#ffff00',
	'#5c5cff', '#ff00ff', '#00ffff', '#ffffff',
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

export interface TerminalOptions {
	/** Width of the backing canvas in pixels. @default 1280 */
	width?: number;
	/** Height of the backing canvas in pixels. @default 720 */
	height?: number;
	/** Font size in pixels. @default 16 */
	fontSize?: number;
	/** Scrollback buffer size (rows). @default 1000 */
	scrollback?: number;
	/** xterm.js theme. */
	theme?: ITheme;
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
	#theme: ITheme;
	#scrollOffset = 0;
	/** Set whenever the buffer/cursor/scroll changes; cleared after render(). */
	#dirty = true;

	constructor(opts: TerminalOptions = {}) {
		const width = opts.width ?? SCREEN_WIDTH;
		const height = opts.height ?? SCREEN_HEIGHT;
		this.#fontSize = opts.fontSize ?? 16;
		this.#theme = opts.theme ?? {};

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
		this.#charWidth = charWidth;
		this.#lineHeight = Math.ceil(this.#fontSize * 1.25);

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

	/** The underlying headless xterm.js instance. */
	get terminal(): XTerminal {
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

	#cellColor(colorCode: number, isBold: boolean, isDefault: boolean): string {
		const fg = this.#theme.foreground ?? '#ffffff';
		if (isDefault || colorCode === -1) return fg;
		if (colorCode >= 0 && colorCode <= 15) {
			if (isBold && colorCode < 8) return ANSI_COLORS[colorCode + 8]!;
			return ANSI_COLORS[colorCode]!;
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

		for (let y = 0; y < rows; y++) {
			const line = buff.getLine(startRow + y);
			if (!line) continue;
			for (let x = 0; x < line.length; x++) {
				line.getCell(x, cell);
				const char = cell.getChars();

				// Cell background (skip default — already cleared).
				if (!cell.isBgDefault()) {
					ctx.fillStyle = this.#cellColor(cell.getBgColor(), false, false);
					ctx.fillRect(x * cw, y * lh, cw, lh);
				}

				if (!char) continue;
				const bold = cell.isBold() !== 0;
				ctx.fillStyle = this.#cellColor(
					cell.getFgColor(),
					bold,
					cell.isFgDefault(),
				);
				ctx.font = bold ? boldFont : baseFont;
				ctx.fillText(char, x * cw, y * lh);
			}
		}

		// Cursor (only when viewing the latest output).
		if (offset === 0) {
			ctx.fillStyle = this.#theme.cursor ?? '#ffffff';
			ctx.globalAlpha = 0.5;
			ctx.fillRect(buff.cursorX * cw, buff.cursorY * lh, cw, lh);
			ctx.globalAlpha = 1;
		}
		return true;
	}
}
