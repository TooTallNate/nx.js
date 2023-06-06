import { Switch } from './switch';
import { INTERNAL_SYMBOL } from './types';

export class Console {
	[INTERNAL_SYMBOL]: Switch;

	constructor(s: Switch) {
		this[INTERNAL_SYMBOL] = s;
		this.log = this.log.bind(this);
		this.warn = this.warn.bind(this);
	}

	log(input: unknown) {
		if (arguments.length === 0) input = '';
		const s = this[INTERNAL_SYMBOL];
		s.print(`${typeof input === 'string' ? input : s.inspect(input)}\n`);
	}

	warn(input: unknown) {
		return this.log(input);
	}
}
