import { $ } from '../$';
import { assertInternalConstructor, stub } from '../utils';

let init = false;

export class Application {
	declare id: bigint;
	declare type: number;
	declare nacp: ArrayBuffer;
	declare icon: ArrayBuffer;

	constructor() {
		assertInternalConstructor(arguments);
	}

	launch(): never {
		stub();
	}
}
$.nsAppInit(Application);

function _init() {
	if (init) return;
	addEventListener('unload', $.nsInitialize());
	init = true;
}

export const applications = {
	*[Symbol.iterator]() {
		_init();
		let offset = 0;
		while (1) {
			const app = $.nsApplicationRecord(offset++);
			if (!app) break;
			Object.setPrototypeOf(app, Application.prototype);
			yield app;
		}
	},
};
