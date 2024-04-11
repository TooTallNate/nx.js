import { $ } from '../$';
import { FsDev } from './fsdev';
import { crypto } from '../crypto';
import { inspect } from '../inspect';
import { readFileSync } from '../fs';
import { proto, stub } from '../utils';
import type { Profile } from './profile';

let init = false;
let self: Application | undefined;

function _init() {
	if (init) return;
	addEventListener('unload', $.nsInitialize());
	init = true;
}

const genName = () => `s${crypto.randomUUID().replace(/-/g, '').slice(0, 16)}`;

/**
 * Represents an installed application (game) on the console,
 * or a homebrew application (`.nro` file).
 *
 * Can be used as an iterator to retrieve the list of installed applications.
 *
 * @example
 *
 * ```typescript
 * for (const app of Switch.Application) {
 *   console.log(app.name);
 * }
 * ```
 */
export class Application {
	/**
	 * The 64-bit unique identifier of the application (`PresenceGroupId`).
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
	 * The version of the application.
	 */
	declare readonly version: string;

	/**
	 * The author or publisher of the application.
	 */
	declare readonly author: string;

	/**
	 * Creates an `Application` instance from the ID of an
	 * installed application.
	 *
	 * @example
	 *
	 * ```typescript
	 * const app = new Switch.Application(0x100bc0018138000n);
	 * console.log(app.name);
	 * ```
	 *
	 * @param id The ID of the installed application.
	 */
	constructor(id: bigint);
	/**
	 * Creates an `Application` instance from an `ArrayBuffer`
	 * containing the contents of a `.nro` homebrew application.
	 *
	 * @example
	 *
	 * ```typescript
	 * const nro = await Switch.readFile('sdmc:/hbmenu.nro');
	 * const app = new Switch.Application(nro);
	 * console.log(app.name);
	 * ```
	 *
	 * @param nro The contents of the `.nro` file.
	 */
	constructor(nro: ArrayBuffer);
	constructor(v: bigint | ArrayBuffer) {
		_init();
		return proto($.nsAppNew(v), Application);
	}

	/**
	 * Launches the application.
	 *
	 * @note This only works for installed applications (__not__ homebrew apps).
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
		$.fsdevCreateSaveData(
			1, // FsSaveDataType_Account
			this.nacp,
			profile.uid,
		);
	}

	/**
	 * Creates the Cache storage for this {@link Application} for the specified save index ID.
	 *
	 * @param index The save index ID. Defaults to `0`.
	 */
	createCacheData(index = 0) {
		$.fsdevCreateSaveData(
			5, // FsSaveDataType_Cache
			this.nacp,
			[0n, 0n],
			index,
		);
	}

	/**
	 * Mounts the save data for this application such that filesystem operations may be used.
	 *
	 * @example
	 *
	 * ```typescript
	 * const profile = Switch.currentProfile({ required: true });
	 * const saveData = app.mountSaveData(profile);
	 *
	 * // Use the filesystem functions to do operations on the save mount
	 * console.log(Switch.readDirSync(saveData.url));
	 *
	 * // Make sure to use `saveData.commit()` after any write operations
	 * const saveStateUrl = new URL('state', saveData.url)
	 * Switch.writeFileSync(saveStateUrl, 'your app state…');
	 * saveData.commit();
	 * ```
	 *
	 * @param profile The {@link Profile} which the save data belongs to.
	 * @param name The name of the device mount for filesystem paths. If not provided, a random name is generated.
	 */
	mountSaveData(profile: Profile, name = genName()): FsDev {
		$.fsdevMountSaveData(name, this.nacp, profile.uid);
		return new FsDev(name);
	}

	/**
	 * Mounts the Cache storage for this application such that filesystem operations may be used.
	 *
	 * @param index The save index ID. Defaults to `0`.
	 * @param name The name of the device mount for filesystem paths. If not provided, a random name is generated.
	 */
	mountCacheData(index = 0, name = genName()): FsDev {
		$.fsdevMountCacheStorage(name, this.nacp, index);
		return new FsDev(name);
	}

	/**
	 * An `Application` instance representing the currently running application.
	 */
	static get self(): Application {
		if (!self) {
			_init();
			let nro: ArrayBuffer | null = null;
			if ($.argv.length) {
				nro = readFileSync($.argv[0]);
			}
			self = proto($.nsAppNew(nro), Application);
		}
		return self;
	}

	static *[Symbol.iterator]() {
		_init();
		let offset = 0;
		while (1) {
			const id = $.nsAppNext(offset++);
			if (!id) break;
			yield new Application(id);
		}
	}
}
$.nsAppInit(Application);

Object.defineProperty(Application.prototype, inspect.keys, {
	enumerable: false,
	value: () => ['id', 'nacp', 'icon', 'name', 'version', 'author'],
});
