import { $ } from '../$';
import { URL } from '../polyfills/url';
import { crypto } from '../crypto';
import { inspect } from '../inspect';
import { assertInternalConstructor, first, proto, stub } from '../utils';
import type { ProfileUid } from './profile';

const genName = () => `s${crypto.randomUUID().replace(/-/g, '').slice(0, 16)}`;

export interface SaveDataFilter {
	spaceId?: number;
	type?: number;
	uid?: ProfileUid;
	systemId?: bigint;
	applicationId?: bigint;
	index?: number;
	rank?: number;
}

export interface SaveDataCreationInfoBase {
	spaceId: number;
	type: number;
	size: bigint;
	journalSize: bigint;
	uid?: ProfileUid;
	systemId?: bigint;
	applicationId?: bigint;
	index?: number;
	rank?: number;
}

export interface SaveDataCreationInfoWithNacp {
	spaceId: number;
	type: number;
	size?: bigint;
	journalSize?: bigint;
	uid?: ProfileUid;
	index?: number;
	rank?: number;
}

export type SaveDataCreationInfo =
	| SaveDataCreationInfoBase
	| SaveDataCreationInfoWithNacp;

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
	 * @private
	 */
	constructor() {
		assertInternalConstructor(arguments);
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
	 * Deletes the save data store.
	 *
	 * @warn This is a destructive operation! Use caution when using this function to avoid accidental data loss.
	 */
	delete() {
		stub();
	}

	/**
	 * Grows a save data store to the requested `dataSize` and `journalSize`.
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
		$.saveDataMount(this, name);
		this.url = new URL(`${name}:/`);
		return this.url;
	}

	/**
	 * Unmounts the filesystem mount. Any filesytem operations attempting to use the mount path in the future will throw an error.
	 *
	 * @example
	 *
	 * ```typescript
	 * Switch.readDirSync(saveData.url); // OK
	 *
	 * saveData.unmount();
	 *
	 * Switch.readDirSync(saveData.url); // ERROR THROWN!
	 * ```
	 */
	unmount() {
		stub();
	}

	freeSpace(): bigint {
		stub();
	}

	totalSpace(): bigint {
		stub();
	}

	static createSync(init: SaveDataCreationInfoBase): SaveData;
	static createSync(
		init: SaveDataCreationInfoWithNacp,
		nacp: ArrayBuffer,
	): SaveData;
	static createSync(init: SaveDataCreationInfo, nacp?: ArrayBuffer): SaveData {
		$.saveDataCreateSync(init, nacp);
		const s = SaveData.find(init);
		if (!s) {
			throw new Error('Could not find newly created save data');
		}
		return s;
	}

	static *filter(filter: SaveDataFilter) {
		// TODO: `saveDataFilter` doesn't seem to work correctly :/
		//const it = $.saveDataFilter(filter);
		//while (1) {
		//	const info = $.fsSaveDataInfoReaderNext(it);
		//	if (!info) break;
		//	yield proto(info, SaveData);
		//}
		for (const s of SaveData) {
			if (typeof filter.spaceId === 'number' && s.spaceId !== filter.spaceId) {
				continue;
			}
			if (typeof filter.type === 'number' && s.type !== filter.type) continue;
			if (
				typeof filter.applicationId === 'bigint' &&
				s.applicationId !== filter.applicationId
			)
				continue;
			if (
				filter.uid &&
				(s.uid[0] !== filter.uid[0] || s.uid[1] !== filter.uid[1])
			)
				continue;
			if (typeof filter.index === 'number' && s.index !== filter.index)
				continue;
			if (typeof filter.rank === 'number' && s.rank !== filter.rank) continue;
			yield s;
		}
	}

	static find(filter: SaveDataFilter): SaveData | undefined {
		return first(SaveData.filter(filter));
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
