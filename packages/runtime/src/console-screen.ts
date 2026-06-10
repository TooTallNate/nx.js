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
// Set when console output (or a scroll) has happened that the screen hasn't
// been re-blitted for yet. Tracked separately from `Terminal.dirty` because
// reading `console.canvas` renders + clears the terminal's dirty flag, so by
// the time presentConsole() runs `render()` may already return false even
// though the on-screen blit is stale. Cleared after a successful blit.
let needsBlit = false;

// Set once the app has read the global `console.canvas`. Apps that composite
// the console themselves often cache that canvas reference once and keep
// drawing it (see apps/console-screen), relying on the per-frame present to
// keep its pixels current after they take over the screen. Apps that NEVER
// read `console.canvas` can't be drawing it, so for them the per-frame
// terminal re-render under app ownership is pure waste (re-rasterizing
// invisible pixels every frame an app logs — a measurable frame-time cost on
// constrained targets). This flag is how presentConsole() distinguishes the
// two. Sticky by design: once observed, liveness is maintained forever.
let consoleCanvasObserved = false;

/** Called by the global console's `canvas` getter on first/every read. */
export function markConsoleCanvasObserved(): void {
	consoleCanvasObserved = true;
}

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
			needsBlit = true; // the viewport changed; re-blit next frame
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
	needsBlit = false; // the app draws the screen now; nothing for us to blit
}

/**
 * Called by the global console on output (or scroll). Records the terminal to
 * present and ensures the screen is in canvas mode so the per-frame present can
 * blit it.
 */
export function onConsoleOutput(term: Terminal): void {
	// Always track the terminal so presentConsole() keeps console.canvas live
	// for apps that composite it (even when they own the screen). Only the
	// screen-mode acquisition + auto-present below is gated on app ownership.
	activeTerminal = term;
	needsBlit = true;
	if (appOwnsScreen) return;
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
 *
 * Once the app owns the screen we no longer blit, but we STILL render the
 * terminal each frame: an app may composite `console.canvas` itself (e.g.
 * `drawImage`), and apps commonly cache the canvas reference once — so the
 * pixels must keep updating as new `console.log` output arrives, even though we
 * stop drawing it to the screen. `render()` is a no-op when nothing changed, so
 * this is cheap.
 */
export function presentConsole(): void {
	if (!activeTerminal) return;
	if (appOwnsScreen) {
		// App owns the screen. Two cases:
		//
		// 1. The app has read `console.canvas` at some point — it may be
		//    compositing a CACHED reference each frame (apps/console-screen
		//    does exactly this), so keep the terminal pixels current with the
		//    eager render. render() is a no-op unless output made it dirty.
		//
		// 2. The app has NEVER read `console.canvas` — nothing can be drawing
		//    it, so skip the eager render. Without this, an app that logs
		//    every frame (e.g. per-tick telemetry) re-rasterizes the whole
		//    invisible console text canvas every frame: a real, hard-to-spot
		//    frame-time cost on constrained targets like Switch. If the app
		//    reads `console.canvas` later, that getter renders on demand and
		//    flips `consoleCanvasObserved`, restoring case-1 liveness.
		if (consoleCanvasObserved) activeTerminal.render();
		return;
	}
	if (!screenCtx) return;
	// Ensure the terminal pixels are current (no-op if already rendered, e.g.
	// because user code read `console.canvas` this turn).
	activeTerminal.render();
	// Blit when output/scroll happened since the last blit. We gate on
	// `needsBlit` rather than render()'s return because reading `console.canvas`
	// can have already done (and cleared) the render — in which case render()
	// returns false here yet the on-screen copy is still stale.
	if (needsBlit) {
		screenCtx.drawImage(activeTerminal.canvas, 0, 0);
		needsBlit = false;
	}
}

/** Whether the app has taken over screen rendering from the console. */
export function isAppOwningScreen(): boolean {
	return appOwnsScreen;
}
