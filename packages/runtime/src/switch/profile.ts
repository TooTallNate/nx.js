import { $ } from '../$';
import { inspect } from '../inspect';
import { proto } from '../utils';

let init = false;

function _init() {
	if (init) return;
	addEventListener('unload', $.accountInitialize());
	init = true;
}

export type ProfileUid = [bigint, bigint];

/**
 * Represents a user profile that exists on the system.
 */
export class Profile {
	/**
	 * The unique ID of the profile, represented as an array of two `bigint` values.
	 */
	declare readonly uid: ProfileUid;

	/**
	 * The human readable nickname of the profile.
	 */
	declare readonly nickname: string;

	/**
	 * The raw JPEG data for the profile image. Can be decoded with the `Image` class.
	 */
	declare readonly image: ArrayBuffer;

	/**
	 * Creates a new `Profile` instance from the given profile UID.
	 *
	 * @example
	 *
	 * ```typescript
	 * const profile = new Switch.Profile([
	 *   0x10005d4864d166b7n,
	 *   0x965b8cb028cd8a81n,
	 * ]);
	 * console.log(profile.nickname);
	 * ```
	 */
	constructor(uid: ProfileUid) {
		_init();
		return proto($.accountProfileNew(uid), Profile);
	}

	/**
	 * Can be used as an iterator to retrieve the list of user profiles.
	 *
	 * @example
	 *
	 * ```typescript
	 * for (const profile of Switch.Profile) {
	 *   console.log(profile.nickname);
	 * }
	 * ```
	 */
	*[Symbol.iterator]() {
		_init();
		const pr = $.accountProfiles();
		for (const p of pr) {
			yield proto(p, Profile);
		}
	}
}
$.accountProfileInit(Profile);

Object.defineProperty(Profile.prototype, inspect.keys, {
	enumerable: false,
	value: () => ['uid', 'nickname', 'image'],
});

let p: Profile | null = null;

export interface CurrentProfileOptions {
	required?: boolean;
}

/**
 * Return a {@link Profile} instance if there was a preselected user
 * when launching the application, or `null` if there was none.
 *
 * If `required: true` is set and there was no preselected user, then
 * the user selection interface will be shown to allow the user to
 * select a profile. Subsequent calls to `currentProfile()` will
 * return the selected profile without user interaction.
 */
export function currentProfile(
	opts: CurrentProfileOptions & { required: true },
): Profile;
export function currentProfile(opts?: CurrentProfileOptions): Profile | null;
export function currentProfile({ required }: CurrentProfileOptions = {}) {
	_init();
	if (p) return p;
	p = $.accountCurrentProfile();
	if (p) proto(p, Profile);
	while (!p && required) p = selectProfile();
	return p;
}

/**
 * Shows the user selection interface and returns a {@link Profile}
 * instance representing the user that was selected.
 *
 * @note This function blocks the event loop until the user has made their selection.
 */
export function selectProfile() {
	_init();
	const p = $.accountSelectProfile();
	if (p) proto(p, Profile);
	return p;
}
