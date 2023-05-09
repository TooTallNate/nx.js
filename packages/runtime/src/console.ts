import { SWITCH_SYMBOL, Switch } from './switch';

export class Console {
	[SWITCH_SYMBOL]: Switch;

	constructor(s: Switch) {
		this[SWITCH_SYMBOL] = s;
	}

	log = (input: unknown) => {
		this[SWITCH_SYMBOL].print(
			`${typeof input === 'string' ? input : JSON.stringify(input)}\n`
		);
	};
}
