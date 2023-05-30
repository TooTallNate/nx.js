import { Switch } from './switch';
import { INTERNAL_SYMBOL } from './types';

export class Console {
	[INTERNAL_SYMBOL]: Switch;

	constructor(s: Switch) {
		this[INTERNAL_SYMBOL] = s;
	}

	log = function log(this: Console, input: unknown) {
		if (arguments.length === 0) input = '';
		this[INTERNAL_SYMBOL].print(
			`${typeof input === 'string' ? input : JSON.stringify(input)}\n`
		);
	};
}
