import { $ } from '../$';
import { inspect } from './inspect';
import { FunctionPrototypeWithIteratorHelpers, proto } from '../utils';

let init = false;

function _init() {
	if (init) return;
	addEventListener('unload', $.accountInitialize());
	init = true;
}

export type ProfileUid = [bigint, bigint];

// The currently selected profile
let p: Profile | null | undefined;

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
	 * Gets or sets the "current" user profile.
	 *
	 * Initially, this value will correspond to the user profile that was selected
	 * via the user selection interface when the application was launched.
	 *
	 * Will be `null` if no user profile is currently selected.
	 *
	 * @example
	 *
	 * ```typescript
	 * const profile = Switch.Profile.current;
	 * if (profile) {
	 *   console.log(`Current user: ${profile.nickname}`);
	 * } else {
	 *   console.log('No user is currently selected');
	 * }
	 * ```
	 */
	static set current(v: Profile | null) {
		p = v;
	}
	static get current(): Profile | null {
		if (typeof p !== 'undefined') return p;
		_init();
		p = $.accountCurrentProfile();
		return p;
	}

	/**
	 * Shows the user selection interface and returns a {@link Profile}
	 * instance representing the user that was selected.
	 *
	 * @note This function blocks the event loop until the user has made their selection.
	 */
	static select() {
		_init();
		const p = $.accountSelectProfile();
		if (p) proto(p, Profile);
		return p;
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
	static *[Symbol.iterator]() {
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

Object.setPrototypeOf(Profile, FunctionPrototypeWithIteratorHelpers);
