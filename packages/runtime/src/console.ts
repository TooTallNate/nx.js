import { INTERNAL_SYMBOL, Switch } from './switch';

export class Console {
	[INTERNAL_SYMBOL]: Switch;

	constructor(s: Switch) {
		this[INTERNAL_SYMBOL] = s;
	}

	log = (input: unknown) => {
		this[INTERNAL_SYMBOL].print(
			`${typeof input === 'string' ? input : JSON.stringify(input)}\n`
		);
	};
}
