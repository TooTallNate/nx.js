import { bgRgb, bold, red, yellow } from 'kleur/colors';
import { $ } from './$';
import { inspect } from './switch/inspect';

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

export interface ConsoleOptions {
	print?(s: string): void;
	printErr?(s: string): void;
}

export class Console {
	#print: (s: string) => void;
	#printErr: (s: string) => void;
	#counts: Map<string, number>;
	#timers: Map<string, number>;
	#groupDepth: number;

	constructor(opts: ConsoleOptions = {}) {
		this.#print = opts.print || $.print;
		this.#printErr = opts.printErr || $.printErr;
		this.#counts = new Map();
		this.#timers = new Map();
		this.#groupDepth = 0;
	}

	/**
	 * Prints string `s` to the console on the screen, without any formatting applied.
	 * A newline is _not_ appended to the end of the string.
	 *
	 * @param s The text to print to the console.
	 */
	print = (s: string) => {
		this.#print.call(this, s);
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
