import { $ } from './$';
import { assertInternalConstructor } from './utils';
import type { env } from './switch';

/**
 * A Map-like object providing methods to interact with the environment variables of the process.
 *
 * Use {@link env | `Switch.env`} to access the singleton instance of this class.
 */
export class Env {
	/**
	 * @private
	 */
	constructor() {
		assertInternalConstructor(arguments);
	}

	get(name: string) {
		return $.getenv(name);
	}

	set(name: string, value: string) {
		$.setenv(name, value);
	}

	delete(name: string) {
		$.unsetenv(name);
	}

	toObject() {
		return $.envToObject();
	}
}
