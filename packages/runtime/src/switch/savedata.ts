import { $ } from '../$';
import { URL } from '../polyfills/url';
import { crypto } from '../crypto';
import type { ProfileUid } from './profile';
import { proto, stub } from '../utils';
import { inspect } from '../inspect';

const genName = () => `s${crypto.randomUUID().replace(/-/g, '').slice(0, 16)}`;

export interface SaveDataFilter {
	type?: number;
	uid?: ProfileUid;
	systemId?: bigint;
	applicationId?: bigint;
	index?: number;
	rank?: number;
}

export type SaveDataInit = {};

/**
 * Represents a "save data store".
 */
export class SaveData {
	/**
	 * A `URL` instance that points to the root of the filesystem mount.
	 * You should use this to create file path references within
	 * the filesystem mount.
	 *
	 * @example
	 *
	 * ```typescript
	 * const dataUrl = new URL('data.json', saveData.url);
	 * ```
	 */
	declare url: URL | null;

	declare readonly id: bigint;
	declare readonly spaceId: number;
	declare readonly type: number;
	declare readonly uid: ProfileUid;
	declare readonly systemId: bigint;
	declare readonly applicationId: bigint;
	declare readonly size: bigint;
	declare readonly index: number;
	declare readonly rank: number;

	/**
	 * Creates a new `SaveData` instance with manually provided values.
	 *
	 * @note This does not create new save data - the values should represent an existing save data.
	 */
	constructor(spaceId: number, id: bigint);
	constructor(spaceId: number, filter: SaveDataFilter);
	constructor(spaceId: number, init: bigint | SaveDataFilter) {
		return proto($.saveDataNew(spaceId, init), SaveData);
	}

	/**
	 * Commits to the disk any write operations that have occurred on this filesystem mount since the previous commit.
	 *
	 * Failure to call this function after writes will cause the data to be lost after the application exits.
	 *
	 * @example
	 *
	 * ```typescript
	 * const saveStateUrl = new URL('state', saveData.url);
	 * Switch.writeFileSync(saveStateUrl, 'my application state...');
	 *
	 * saveData.commit(); // Write operation is persisted to the disk
	 * ```
	 */
	commit() {
		stub();
	}

	/**
	 *
	 */
	delete() {
		stub();
	}

	/**
	 *
	 * @param dataSize
	 * @param journalSize
	 */
	extend(dataSize: bigint, journalSize: bigint) {
		stub();
	}

	/**
	 * Mounts the save data such that filesystem operations may be used.
	 *
	 * @param name The name of the mount for filesystem paths. By default, a random name is generated. Shouldn't exceed 31 characters, and shouldn't have a trailing colon.
	 */
	mount(name = genName()) {
		$.fsdevMount(this, name);
		this.url = new URL(`${name}:/`);
		return this.url;
	}

	/**
	 * Unmounts the filesystem mount. Any filesytem operations attempting to use the mount path in the future will throw an error.
	 *
	 * @example
	 *
	 * ```typescript
	 * Switch.readFileSync(device.url); // OK
	 *
	 * device.unmount();
	 *
	 * Switch.readFileSync(device.url); // ERROR THROWN!
	 * ```
	 */
	unmount() {
		stub();
	}

	static createSync(spaceId: number, init: SaveDataInit): SaveData {
		const s = $.saveDataCreate(spaceId, init);
		return proto(s, SaveData);
	}

	static *[Symbol.iterator]() {
		const spaceIds = [0, 1, 2, 3, 4, 100, 101]; // `FsSaveDataSpaceId`
		for (const spaceId of spaceIds) {
			const it = $.fsOpenSaveDataInfoReader(spaceId);
			if (!it) continue;
			while (1) {
				const info = $.fsSaveDataInfoReaderNext(it);
				if (!info) break;
				yield proto(info, SaveData);
			}
		}
	}
}
$.saveDataInit(SaveData);

Object.defineProperty(SaveData.prototype, inspect.keys, {
	enumerable: false,
	value: () => [
		'id',
		'spaceId',
		'type',
		'uid',
		'systemId',
		'applicationId',
		'size',
		'index',
		'rank',
	],
});
