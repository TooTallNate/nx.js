import type { Terminal } from './terminal';
import type { CanvasRenderingContext2D } from './canvas/canvas-rendering-context-2d';

// Drives presenting the GLOBAL console's terminal canvas onto `screen` each
// frame. The console renders into its own OffscreenCanvas; this module blits
// that canvas to the screen whenever it changes — but only while the app hasn't
// taken over screen rendering itself. Once user code acquires the screen 2D
// context for its own drawing (`screen.getContext('2d')`), the auto-present
// stops and the app is responsible for compositing `console.canvas` if desired.

let activeTerminal: Terminal | undefined;
let appOwnsScreen = false;
let screenCtx: CanvasRenderingContext2D | undefined;

// `screen.ts` registers a getter that returns the screen's own 2D context +
// puts the screen into canvas (framebuffer) mode, WITHOUT marking the app as
// owning the screen, plus the `screen` EventTarget so the console can wire up
// touch-drag scrollback. Lazily set to avoid an import cycle.
let consoleScreenCtxGetter: (() => CanvasRenderingContext2D | undefined) | undefined;
let consoleScreenTarget: EventTarget | undefined;
let touchWired = false;

export function registerConsoleScreen(
	getter: () => CanvasRenderingContext2D | undefined,
	screen: EventTarget,
): void {
	consoleScreenCtxGetter = getter;
	consoleScreenTarget = screen;
}

// Map a vertical touch drag on the screen to console scrollback. ~1 row per
// `lineHeight` of drag. The gesture matches grabbing the content and moving it:
// dragging DOWN pulls older lines into view (scrolls back into history), and
// dragging UP returns toward the latest output.
const TOUCH_ROW_PX = 20;
function wireTouchScroll(): void {
	if (touchWired || !consoleScreenTarget) return;
	touchWired = true;
	let lastY: number | null = null;
	const target = consoleScreenTarget as any;
	target.addEventListener('touchstart', (e: any) => {
		if (appOwnsScreen) return;
		lastY = e.touches?.[0]?.screenY ?? null;
	});
	target.addEventListener('touchmove', (e: any) => {
		if (appOwnsScreen || lastY == null || !activeTerminal) return;
		const y = e.touches?.[0]?.screenY;
		if (typeof y !== 'number') return;
		const dy = y - lastY;
		const rows = (dy / TOUCH_ROW_PX) | 0;
		if (rows !== 0) {
			// screenY increases downward, so dragging DOWN gives a positive
			// `rows` -> scrollUp() into history; dragging UP gives a negative
			// `rows` -> scrollUp(negative) == scrollDown() toward the latest
			// output. (scrollUp/scrollDown clamp internally.)
			activeTerminal.scrollUp(rows);
			lastY = y;
		}
	});
	target.addEventListener('touchend', () => {
		lastY = null;
	});
}

/**
 * Called when user code takes ownership of the screen for its own rendering
 * (`Screen.getContext('2d')`). Stops the console auto-present so it doesn't
 * overwrite the app's frames; the app may still composite `console.canvas`.
 */
export function markAppOwnsScreen(): void {
	appOwnsScreen = true;
	screenCtx = undefined;
}

/**
 * Called by the global console on output (or scroll). Records the terminal to
 * present and ensures the screen is in canvas mode so the per-frame present can
 * blit it.
 */
export function onConsoleOutput(term: Terminal): void {
	if (appOwnsScreen) return;
	activeTerminal = term;
	if (!screenCtx && consoleScreenCtxGetter) {
		try {
			screenCtx = consoleScreenCtxGetter();
			wireTouchScroll();
		} catch {
			screenCtx = undefined;
		}
	}
}

/**
 * Per-frame hook (called from the runtime frame handler, after rAF). Blits the
 * active console terminal canvas to the screen when it has changed.
 */
export function presentConsole(): void {
	if (appOwnsScreen || !activeTerminal || !screenCtx) return;
	// render() is a no-op when nothing changed; only blit on an actual change.
	if (activeTerminal.render()) {
		screenCtx.drawImage(activeTerminal.canvas, 0, 0);
	}
}

/** Whether the app has taken over screen rendering from the console. */
export function isAppOwningScreen(): boolean {
	return appOwnsScreen;
}
