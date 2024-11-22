import { $ } from '../$';
import { URL } from '../polyfills/url';
import { crypto } from '../crypto';
import { inspect } from './inspect';
import {
	assertInternalConstructor,
	FunctionPrototypeWithIteratorHelpers,
	proto,
	stub,
} from '../utils';
import type { ProfileUid } from './profile';

const genName = () => `s${crypto.randomUUID().replace(/-/g, '').slice(0, 16)}`;

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

function matches(
	saveData: SaveData,
	info: Omit<SaveDataCreationInfoBase, 'size' | 'journalSize'>,
) {
	if (typeof info.spaceId === 'number' && saveData.spaceId !== info.spaceId) {
		return false;
	}
	if (typeof info.type === 'number' && saveData.type !== info.type) {
		return false;
	}
	if (
		typeof info.applicationId === 'bigint' &&
		saveData.applicationId !== info.applicationId
	) {
		return false;
	}
	if (
		info.uid &&
		(saveData.uid[0] !== info.uid[0] || saveData.uid[1] !== info.uid[1])
	) {
		return false;
	}
	if (typeof info.index === 'number' && saveData.index !== info.index) {
		return false;
	}
	if (typeof info.rank === 'number' && saveData.rank !== info.rank) {
		return false;
	}
	return true;
}

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
	 * > CAUTION: This is a destructive operation! Use caution when using
	 * > this function to avoid accidental data loss, such as prompting the
	 * > user to confirm the deletion.
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
	 * @param name The name of the mount for filesystem paths. By default, a random name is generated. Should not exceed 31 characters, and should not have a trailing colon.
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
		const s = Iterator.from(SaveData).find((s) => matches(s, init));
		if (!s) {
			throw new Error('Could not find newly created save data');
		}
		return s;
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

Object.setPrototypeOf(SaveData, FunctionPrototypeWithIteratorHelpers);
