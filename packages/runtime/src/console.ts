import { bgRgb, bold, red, yellow } from 'kleur/colors';
import { $ } from './$';
import { inspect } from './switch/inspect';
import { createInternal } from './utils';

const bgRedDim = bgRgb(60, 0, 0);
const bgYellowDim = bgRgb(60, 60, 0);

const formatters: Record<string, (v: unknown) => string> = {
	s(v) {
		return String(v);
	},
	d(v) {
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

const _ = createInternal<Console, Required<ConsoleOptions>>();

export class Console {
	constructor(opts: ConsoleOptions = {}) {
		_.set(this, {
			print: opts.print || $.print,
			printErr: opts.print || $.printErr,
		});
	}

	/**
	 * Prints string `s` to the console on the screen, without any formatting applied.
	 * A newline is _not_ appended to the end of the string.
	 *
	 * @param s The text to print to the console.
	 */
	print = (s: string) => {
		_(this).print.call(this, s);
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
		_(this).printErr.call(this, s);
	};

	/**
	 * Logs the formatted `input` to the screen as white text.
	 */
	log = (...input: unknown[]) => {
		this.print(`${format(...input)}\n`);
	};

	/**
	 * Logs the formatted `input` to the screen as yellow text.
	 */
	warn = (...input: unknown[]) => {
		this.print(`${bold(bgYellowDim(yellow(format(...input))))}\n`);
	};

	/**
	 * Logs the formatted `input` to the screen as red text.
	 */
	error = (...input: unknown[]) => {
		this.print(`${bold(bgRedDim(red(format(...input))))}\n`);
	};

	/**
	 * Writes the formatted `input` to the debug log file.
	 *
	 * > TIP: This function **does not** invoke _text rendering mode_, so it can safely be used when rendering with the Canvas API.
	 */
	debug = (...input: unknown[]) => {
		this.printErr(`${format(...input)}\n`);
	};

	/**
	 * Logs the formatted `input` to the screen as white text,
	 * including a stack trace of where the function was invoked.
	 */
	trace = (...input: unknown[]) => {
		const f = format(...input);
		let s = new Error().stack!.split('\n').slice(1).join('\n');
		if (!s.endsWith('\n')) s += '\n';
		this.print(`Trace${f ? `: ${f}` : ''}\n${s}`);
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
