import { bgRgb, bold, red, yellow } from 'kleur/colors';
import { $ } from './$';
import { inspect } from './switch/inspect';
import { Terminal, consoleFontAvailable } from './terminal';
import type { TerminalOptions } from './terminal';
import type { OffscreenCanvas } from './canvas/offscreen-canvas';
import { onConsoleOutput } from './console-screen';

const bgRedDim = bgRgb(60, 0, 0);
const bgYellowDim = bgRgb(60, 60, 0);

const formatters: Record<string, (v: unknown) => string> = {
	s(v) {
		return String(v);
	},
	d(v) {
		return String(Number(v));
	},
	i(v) {
		return String(Math.trunc(Number(v)));
	},
	f(v) {
		return String(Number(v));
	},
	j(v) {
		return JSON.stringify(v);
	},
	o(v) {
		return inspect(v);
	},
	O(v) {
		return inspect(v);
	},
};

const RE = new RegExp(`\\%([${Object.keys(formatters).join('')}])`, 'g');

function format(...input: unknown[]): string {
	if (input.length === 0) return '';
	let s = '';
	if (typeof input[0] === 'string') {
		const pre = input.shift() as string;
		RE.lastIndex = 0;
		s = pre.replace(RE, (_, f) => {
			return formatters[f](input.shift());
		});
	}
	if (input.length) {
		if (s) s += ' ';
		s += input.map((v) => inspect(v)).join(' ');
	}
	return s;
}

/**
 * Options for a {@link Console}. In addition to the optional `print`/`printErr`
 * sinks, the canvas-backed terminal can be styled via {@link TerminalOptions}
 * (theme colors, font size, scrollback, cursor, etc.).
 *
 * For the global `console`, assign these options to {@link Console.options}
 * before the first output (e.g. at the top of your app) — the underlying
 * terminal is created lazily, so the options take effect on first render:
 *
 * ```ts
 * console.options = {
 *   fontSize: 24,
 *   theme: { background: '#002b36', foreground: '#839496' },
 * };
 * ```
 */
export interface ConsoleOptions extends TerminalOptions {
	print?(s: string): void;
	printErr?(s: string): void;
}

// The global `console` (first Console created without a custom print sink) owns
// the default on-screen terminal present. Further `new Console()` instances are
// isolated — they render to their own `console.canvas` for apps to composite.
let globalConsoleClaimed = false;

export class Console {
	#print: (s: string) => void;
	#printErr: (s: string) => void;
	#counts: Map<string, number>;
	#timers: Map<string, number>;
	#groupDepth: number;
	/** Canvas-backed terminal renderer (lazily created on first output). */
	#terminal?: Terminal;
	/** Whether this is the global `console` (drives the default screen present). */
	#isGlobal: boolean;
	/** Custom print sink (if provided, bypasses the canvas terminal). */
	#customPrint?: (s: string) => void;
	/** Terminal styling options applied when the terminal is (re)created. */
	#options: TerminalOptions;

	constructor(opts: ConsoleOptions = {}) {
		this.#customPrint = opts.print;
		this.#print = opts.print || $.print;
		this.#printErr = opts.printErr || $.printErr;
		this.#counts = new Map();
		this.#timers = new Map();
		this.#groupDepth = 0;
		const { print, printErr, ...termOpts } = opts;
		// The first Console created without a custom `print` sink is the global
		// one — it owns the default on-screen console present (see index.ts).
		this.#isGlobal = !opts.print && !globalConsoleClaimed;
		if (this.#isGlobal) globalConsoleClaimed = true;
		// Seed the global console's styling from the `[console]` section of
		// nxjs.ini ($.config.console) so it can be themed with zero app code.
		// Explicitly-passed constructor options (rare for the global console)
		// and a later `console.options =` assignment both override this.
		this.#options =
			this.#isGlobal && Object.keys(termOpts).length === 0
				? ($.config?.console as TerminalOptions) ?? {}
				: termOpts;
	}

	/**
	 * The terminal styling options (theme, font size, scrollback, cursor, …).
	 * Assigning replaces them and rebuilds the underlying terminal, so a fresh
	 * theme/font takes effect immediately (the scrollback buffer is reset).
	 *
	 * For the global `console`, set this before the first log to style the
	 * on-screen console:
	 *
	 * ```ts
	 * console.options = { theme: { background: '#002b36' }, fontSize: 24 };
	 * ```
	 */
	get options(): TerminalOptions {
		return this.#options;
	}
	set options(opts: TerminalOptions) {
		this.#options = opts ?? {};
		// Drop any existing terminal so the next access rebuilds it with the new
		// options. (Buffered scrollback is not carried over.)
		this.#terminal = undefined;
	}

	/**
	 * The {@link OffscreenCanvas} this console renders its terminal into. Lazily
	 * created on first access (or first output). Apps can composite it onto the
	 * screen, e.g. `screen.getContext('2d').drawImage(console.canvas, x, y)`.
	 */
	get canvas(): OffscreenCanvas {
		return this.#getTerminal().canvas;
	}

	#getTerminal(): Terminal {
		if (!this.#terminal) {
			this.#terminal = new Terminal(this.#options);
		}
		return this.#terminal;
	}

	/**
	 * Write `s` to this console's terminal renderer. Returns false (and the
	 * caller should fall back to `$.print`) when the canvas terminal can't be
	 * used yet (e.g. the Geist Mono font hasn't loaded — early boot, or the
	 * host test binary which has no display).
	 */
	#writeTerminal(s: string): boolean {
		// A custom print sink always takes precedence over the canvas terminal.
		if (this.#customPrint) return false;
		// No canvas terminal when the bundled font can't be loaded (host test
		// binary / very early boot) — fall back to the native libnx console.
		if (!consoleFontAvailable()) return false;
		let term: Terminal;
		try {
			term = this.#getTerminal();
		} catch {
			return false;
		}
		term.write(s);
		if (this.#isGlobal) onConsoleOutput(term);
		return true;
	}

	/**
	 * Prints string `s` to the console on the screen, without any formatting applied.
	 * A newline is _not_ appended to the end of the string.
	 *
	 * @param s The text to print to the console.
	 */
	print = (s: string) => {
		// Prefer the canvas-backed terminal; fall back to the native libnx
		// console ($.print) when it's unavailable (early boot before the font
		// loads, or the host test binary with no display).
		if (!this.#writeTerminal(s)) {
			this.#print.call(this, s);
		}
	};

	/**
	 * Scroll the console's terminal view up `n` rows into the scrollback
	 * history. No-op if this console isn't using the canvas renderer.
	 */
	scrollUp = (n = 1) => {
		this.#terminal?.scrollUp(n);
		if (this.#isGlobal && this.#terminal) onConsoleOutput(this.#terminal);
	};

	/**
	 * Scroll the console's terminal view down `n` rows toward the latest output.
	 */
	scrollDown = (n = 1) => {
		this.#terminal?.scrollDown(n);
		if (this.#isGlobal && this.#terminal) onConsoleOutput(this.#terminal);
	};

	/**
	 * Prints string `s` to the debug log file, without any formatting applied.
	 * A newline is _not_ appended to the end of the string.
	 *
	 * > TIP: This function **does not** invoke _text rendering mode_, so it can safely be used when rendering with the Canvas API.
	 *
	 * @param s The text to print to the log file.
	 */
	printErr = (s: string) => {
		this.#printErr.call(this, s);
	};

	/**
	 * Logs the formatted `input` to the screen as white text.
	 */
	log = (...input: unknown[]) => {
		const depth = this.#groupDepth;
		const indent = depth > 0 ? '  '.repeat(depth) : '';
		this.print(`${indent}${format(...input)}\n`);
	};

	/**
	 * Logs the formatted `input` to the screen as yellow text.
	 */
	warn = (...input: unknown[]) => {
		const depth = this.#groupDepth;
		const indent = depth > 0 ? '  '.repeat(depth) : '';
		this.print(`${indent}${bold(bgYellowDim(yellow(format(...input))))}\n`);
	};

	/**
	 * Logs the formatted `input` to the screen as red text.
	 */
	error = (...input: unknown[]) => {
		const depth = this.#groupDepth;
		const indent = depth > 0 ? '  '.repeat(depth) : '';
		this.print(`${indent}${bold(bgRedDim(red(format(...input))))}\n`);
	};

	/**
	 * Writes the formatted `input` to the debug log file.
	 *
	 * > TIP: This function **does not** invoke _text rendering mode_, so it can safely be used when rendering with the Canvas API.
	 */
	debug = (...input: unknown[]) => {
		const depth = this.#groupDepth;
		const indent = depth > 0 ? '  '.repeat(depth) : '';
		this.printErr(`${indent}${format(...input)}\n`);
	};

	/**
	 * Logs the formatted `input` to the screen as white text,
	 * including a stack trace of where the function was invoked.
	 */
	trace = (...input: unknown[]) => {
		const depth = this.#groupDepth;
		const indent = depth > 0 ? '  '.repeat(depth) : '';
		const f = format(...input);
		let s = new Error().stack!.split('\n').slice(1).join('\n');
		if (!s.endsWith('\n')) s += '\n';
		this.print(`${indent}Trace${f ? `: ${f}` : ''}\n${s}`);
	};

	/**
	 * Logs the formatted `input` if `condition` is falsy.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/console/assert_static
	 */
	assert = (condition?: boolean, ...args: unknown[]) => {
		if (!condition) {
			if (args.length === 0) {
				this.error('Assertion failed');
			} else {
				this.error(`Assertion failed: ${format(...args)}`);
			}
		}
	};

	/**
	 * Logs the number of times `count()` has been called with the given label.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/console/count_static
	 */
	count = (label = 'default') => {
		const count = (this.#counts.get(label) || 0) + 1;
		this.#counts.set(label, count);
		this.log(`${label}: ${count}`);
	};

	/**
	 * Resets the counter for the given label.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/console/countReset_static
	 */
	countReset = (label = 'default') => {
		this.#counts.delete(label);
	};

	/**
	 * Displays an interactive listing of the properties of the specified object.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/console/dir_static
	 */
	dir = (item?: unknown) => {
		this.log(item);
	};

	/**
	 * Alias for {@link Console.dir}.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/console/dirxml_static
	 */
	dirxml = (item?: unknown) => {
		this.log(item);
	};

	/**
	 * Creates a new inline group, indenting subsequent console messages.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/console/group_static
	 */
	group = (...label: unknown[]) => {
		if (label.length > 0) {
			this.log(...label);
		}
		this.#groupDepth++;
	};

	/**
	 * Alias for {@link Console.group}.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/console/groupCollapsed_static
	 */
	groupCollapsed = (...label: unknown[]) => {
		this.group(...label);
	};

	/**
	 * Exits the current inline group.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/console/groupEnd_static
	 */
	groupEnd = () => {
		if (this.#groupDepth > 0) this.#groupDepth--;
	};

	/**
	 * Displays tabular data as a table. Falls back to {@link Console.log}.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/console/table_static
	 */
	table = (data?: unknown) => {
		this.log(data);
	};

	/**
	 * Starts a timer with the given label.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/console/time_static
	 */
	time = (label = 'default') => {
		this.#timers.set(label, performance.now());
	};

	/**
	 * Logs the elapsed time for the given timer and removes it.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/console/timeEnd_static
	 */
	timeEnd = (label = 'default') => {
		const start = this.#timers.get(label);
		if (start === undefined) {
			this.warn(`Timer '${label}' does not exist`);
			return;
		}
		this.#timers.delete(label);
		this.log(`${label}: ${(performance.now() - start).toFixed(3)}ms`);
	};

	/**
	 * Logs the elapsed time for the given timer without removing it.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/console/timeLog_static
	 */
	timeLog = (label = 'default', ...args: unknown[]) => {
		const start = this.#timers.get(label);
		if (start === undefined) {
			this.warn(`Timer '${label}' does not exist`);
			return;
		}
		const elapsed = `${label}: ${(performance.now() - start).toFixed(3)}ms`;
		if (args.length > 0) {
			this.log(`${elapsed} ${format(...args)}`);
		} else {
			this.log(elapsed);
		}
	};

	/**
	 * Clears the console by printing the ANSI escape code for clearing the terminal.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/console/clear_static
	 */
	clear = () => {
		// ESC[2J clears the screen, ESC[H moves cursor to home position
		this.print('\x1b[2J\x1b[H');
	};

	/**
	 * Outputs a message at the "info" log level. Alias for {@link Console.log}.
	 *
	 * @see https://developer.mozilla.org/docs/Web/API/console/info_static
	 */
	info = (...input: unknown[]) => {
		this.log(...input);
	};
}

/**
 * The global `console` object contains functions for printing text to the
 * screen, which can be useful for text-based applications, and is also
 * useful for debugging.
 *
 * Most methods use the {@link inspect | `Switch.inspect()`} method
 * for formatting, and the {@link console.print | `console.print()`} method
 * to output to the screen.
 *
 * > IMPORTANT: Unless otherwise stated, invoking any method on the `console`
 * > object switches the application to _console rendering mode_, clearing any
 * > pixels previously drawn on the screen using the Canvas API.
 *
 * @see https://developer.mozilla.org/docs/Web/API/console
 */
export var console = new Console();
