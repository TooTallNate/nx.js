import { bgRgb, red, yellow } from 'kleur/colors';
import { Switch } from './switch';
import { INTERNAL_SYMBOL } from './types';

const bgRedDim = bgRgb(30, 0, 0);
const bgYellowDim = bgRgb(30, 30, 0);

export class Console {
	[INTERNAL_SYMBOL]: Switch;

	constructor(s: Switch) {
		this[INTERNAL_SYMBOL] = s;
	}

	// TODO: add support for multiple arguments, printf formatting
	private _format(...input: unknown[]): string {
		if (input.length === 0) return '';
		const s = this[INTERNAL_SYMBOL];
		return typeof input[0] === 'string' ? input[0] : s.inspect(input[0]);
	}

	log = (...input: unknown[]) => {
		const s = this[INTERNAL_SYMBOL];
		s.print(`${this._format(...input)}\n`);
	};

	warn = (...input: unknown[]) => {
		const s = this[INTERNAL_SYMBOL];
		s.print(`${bgYellowDim(yellow(this._format(...input)))}\n`);
	};

	error = (...input: unknown[]) => {
		const s = this[INTERNAL_SYMBOL];
		s.print(`${bgRedDim(red(this._format(...input)))}\n`);
	};
}
