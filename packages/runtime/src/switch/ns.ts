import { $ } from '../$';
import { FsDev } from './fsdev';
import { readFileSync } from '../fs';
import { assertInternalConstructor, stub } from '../utils';
import type { Profile } from './profile';

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
	declare readonly id: bigint;

	/**
	 * The raw NACP data of the application. Use the `@tootallnate/nacp` module to parse this data.
	 */
	declare readonly nacp: ArrayBuffer;

	/**
	 * The raw JPEG data for the cover art of the application. Can be decoded with the `Image` class.
	 */
	declare readonly icon?: ArrayBuffer;

	/**
	 * The name of the application.
	 */
	declare readonly name: string;

	/**
	 * The author or publisher of the application.
	 */
	declare readonly author: string;

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

	/**
	 * Creates the Save Data for this {@link Application} for the provided user profile.
	 *
	 * @example
	 *
	 * ```typescript
	 * const profile = Switch.currentProfile({ required: true });
	 * app.createSaveData(profile);
	 * ```
	 *
	 * @param profile The {@link Profile} to create the save data for.
	 */
	createSaveData(profile: Profile) {
		$.fsdevCreateSaveData(this.nacp, profile.uid);
	}

	/**
	 * Mounts the save data for this application such that filesystem operations may be used.
	 *
	 * @example
	 *
	 * ```typescript
	 * const profile = Switch.currentProfile({ required: true });
	 * const device = app.mountSaveData('save', profile);
	 *
	 * // Use the filesystem functions to do operations on the save mount
	 * console.log(Switch.readDirSync('save:/'));
	 *
	 * // Make sure to use `device.commit()` after any write operations
	 * Switch.writeFileSync('save:/state', 'your app state…');
	 * device.commit();
	 * ```
	 *
	 * @param name The name of the device mount for filesystem paths.
	 * @param profile The {@link Profile} which the save data belongs to.
	 */
	mountSaveData(name: string, profile: Profile) {
		$.fsdevMountSaveData(name, this.nacp, profile.uid);
		return new FsDev(name);
	}
}
$.nsAppInit(Application);

let self: Application | undefined;

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
	/**
	 * An `Application` instance representing the currently running application.
	 */
	get self() {
		if (!self) {
			_init();
			let nro: ArrayBuffer | null = null;
			if ($.argv.length) {
				nro = readFileSync($.argv[0]);
			}
			self = $.nsAppNew(nro);
			Object.setPrototypeOf(self, Application.prototype);
		}
		return self;
	},

	*[Symbol.iterator]() {
		_init();
		let offset = 0;
		while (1) {
			const id = $.nsAppNext(offset++);
			if (!id) break;
			const app = $.nsAppNew(id);
			Object.setPrototypeOf(app, Application.prototype);
			yield app;
		}
	},
};
