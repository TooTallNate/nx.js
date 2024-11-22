import { $ } from '../$';
import { SaveData, SaveDataCreationInfoWithNacp } from './savedata';
import { inspect } from './inspect';
import { readFileSync } from '../fs';
import { FunctionPrototypeWithIteratorHelpers, proto, stub } from '../utils';
import type { Profile } from './profile';

let init = false;
let self: Application | undefined;

function _init() {
	if (init) return;
	addEventListener('unload', $.nsInitialize());
	init = true;
}

const getSaveDataOwnerId = (nacp: DataView) => nacp.getBigUint64(0x3078, true);

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

	createSaveDataSync(info: SaveDataCreationInfoWithNacp) {
		return SaveData.createSync(info, this.nacp);
	}

	/**
	 * Creates the Save Data for this {@link Application} for the provided user profile.
	 *
	 * @example
	 *
	 * ```typescript
	 * app.createProfileSaveDataSync(Switch.Profile.current);
	 * ```
	 *
	 * @param profile The {@link Profile} to create the save data for.
	 */
	createProfileSaveDataSync(
		profile: Profile,
		info?: Partial<SaveDataCreationInfoWithNacp>,
	) {
		return this.createSaveDataSync({
			spaceId: 1 /* FsSaveDataSpaceId_User */,
			type: 1 /* FsSaveDataType_Account */,
			...info,
			uid: profile.uid,
		});
	}

	/**
	 * Creates the Cache storage for this {@link Application} for the specified save index ID.
	 *
	 * @param index The save index ID. Defaults to `0`.
	 */
	createCacheSaveDataSync(
		index = 0,
		info?: Partial<SaveDataCreationInfoWithNacp>,
	) {
		return this.createSaveDataSync({
			spaceId: 1 /* FsSaveDataSpaceId_User */,
			type: 5 /* FsSaveDataType_Cache */,
			...info,
			index,
		});
	}

	*filterSaveData(
		fn?: (saveData: SaveData) => boolean,
	): Generator<SaveData, void> {
		const id = getSaveDataOwnerId(new DataView(this.nacp));
		yield* Iterator.from(SaveData).filter((s) => {
			if (s.applicationId !== id) return false;
			return fn ? fn(s) : true;
		});
	}

	findSaveData(fn?: (saveData: SaveData) => boolean) {
		const { done, value } = this.filterSaveData(fn).next();
		return done ? undefined : value;
	}

	/**
	 * An {@link Application} instance representing the currently running application.
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

Object.setPrototypeOf(Application, FunctionPrototypeWithIteratorHelpers);
