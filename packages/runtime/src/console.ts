import { bgRgb, bold, red, yellow } from 'kleur/colors';
import { Switch } from './switch';
import { INTERNAL_SYMBOL } from './types';

const bgRedDim = bgRgb(60, 0, 0);
const bgYellowDim = bgRgb(60, 60, 0);

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
		s.print(`${bold(bgYellowDim(yellow(this._format(...input))))}\n`);
	};

	error = (...input: unknown[]) => {
		const s = this[INTERNAL_SYMBOL];
		s.print(`${bold(bgRedDim(red(this._format(...input))))}\n`);
	};
}
