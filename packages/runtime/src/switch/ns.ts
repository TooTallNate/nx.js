import { $ } from '../$';
import { assertInternalConstructor, stub } from '../utils';

let init = false;

function _init() {
	if (init) return;
	addEventListener('unload', $.nsInitialize());
	init = true;
}

/**
 * Represents an installed application (game) on the console.
 */
export class Application {
	/**
	 * The 64-bit unique identifier of the application.
	 */
	declare id: bigint;
	/**
	 * The "type" of application.
	 */
	declare type: number;
	/**
	 * The raw NACP data of the application. Use the `@tootallnate/nacp` module to parse this data.
	 */
	declare nacp: ArrayBuffer;
	/**
	 * The raw JPEG data for the cover art of the application. Can be decoded with the `Image` class.
	 */
	declare icon: ArrayBuffer;
	/**
	 * The name of the application.
	 */
	declare name: string;
	/**
	 * The author or publisher of the application.
	 */
	declare author: string;

	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
	}

	/**
	 * Launches the application.
	 */
	launch(): never {
		stub();
	}
}
$.nsAppInit(Application);

/**
 * Can be used as an iterator to retrieve the list of installed applications.
 *
 * @example
 *
 * ```typescript
 * for (const app of Switch.applications) {
 *   console.log(app.name);
 * }
 * ```
 */
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
