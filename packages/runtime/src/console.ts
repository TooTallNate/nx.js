import { bgRgb, bold, red, yellow } from 'kleur/colors';
import type { SwitchClass } from './switch';

declare const Switch: SwitchClass;

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
		return Switch.inspect(v);
	},
	O(v) {
		return Switch.inspect(v);
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
		s += input.map((v) => Switch.inspect(v)).join(' ');
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
 * @see https://developer.mozilla.org/en-US/docs/Web/API/console
 */
export const console = {
	/**
	 * Logs to the screen the formatted `input` as white text.
	 */
	log(...input: unknown[]) {
		Switch.print(`${format(...input)}\n`);
	},

	/**
	 * Logs to the screen the formatted `input` as yellow text.
	 */
	warn(...input: unknown[]) {
		Switch.print(`${bold(bgYellowDim(yellow(format(...input))))}\n`);
	},

	/**
	 * Logs to the screen the formatted `input` as red text.
	 */
	error(...input: unknown[]) {
		Switch.print(`${bold(bgRedDim(red(format(...input))))}\n`);
	},

	/**
	 * `console.debug()` is an alias for `console.log()`.
	 */
	debug(...input: unknown[]) {
		console.log(...input);
	},
};
