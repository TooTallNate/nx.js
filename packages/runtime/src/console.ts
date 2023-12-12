import { bgRgb, bold, red, yellow } from 'kleur/colors';
import { $ } from './$';
import { inspect } from './inspect';
import type { SwitchClass } from './switch';

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

/**
 * The global `console` object contains functions for printing text to the
 * screen, which can be useful for text-based applications, and is also
 * useful for debugging.
 *
 * All methods use the {@link SwitchClass.inspect | `Switch.inspect()`} method
 * for formatting, and the {@link SwitchClass.print | `Switch.print()`} method
 * to output to the screen.
 *
 * > **NOTE:** Invoking any method on the `console` object switches the
 * > application to _text rendering mode_, clearing any pixels previously
 * > drawn on the screen using the Canvas API.
 *
 * @see https://developer.mozilla.org/docs/Web/API/console
 */
export const console = {
	/**
	 * Prints string `s` to the console on the screen, without any formatting applied.
	 * Newline is _not_ appending to the end of the string.
	 *
	 * @param s The text to print to the console.
	 */
	print(s: string) {
		$.print(s);
	},

	/**
	 * Prints string `s` to the debug log file, without any formatting applied.
	 * Newline is _not_ appending to the end of the string.
	 *
	 * @param s The text to print to the log file.
	 * @note This function **does not** invoke _text rendering mode_, so it can safely be used when rendering with the Canvas API.
	 */
	printErr(s: string) {
		$.printErr(s);
	},

	/**
	 * Logs the formatted `input` to the screen as white text.
	 */
	log(...input: unknown[]) {
		$.print(`${format(...input)}\n`);
	},

	/**
	 * Logs the formatted `input` to the screen as yellow text.
	 */
	warn(...input: unknown[]) {
		$.print(`${bold(bgYellowDim(yellow(format(...input))))}\n`);
	},

	/**
	 * Logs the formatted `input` to the screen as red text.
	 */
	error(...input: unknown[]) {
		$.print(`${bold(bgRedDim(red(format(...input))))}\n`);
	},

	/**
	 * Writes the formatted `input` to the debug log file.
	 *
	 * @note This function **does not** invoke _text rendering mode_, so it can safely be used when rendering with the Canvas API.
	 */
	debug(...input: unknown[]) {
		$.printErr(`${format(...input)}\n`);
	},

	/**
	 * Logs the formatted `input` to the screen as white text,
	 * including a stack trace of where the function was invoked.
	 */
	trace(...input: unknown[]) {
		const f = format(...input);
		const s = new Error().stack!.split('\n').slice(1).join('\n');
		$.print(`Trace${f ? `: ${f}` : ''}\n${s}`);
	},
};
