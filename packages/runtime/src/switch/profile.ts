import { $ } from '../$';
import { assertInternalConstructor } from '../utils';

let init = false;

function _init() {
	if (init) return;
	addEventListener('unload', $.accountInitialize());
	init = true;
}

/**
 * Represents a user profile that exists on the system.
 */
export class Profile {
	/**
	 * The unique ID of the profile, represented as an array of two `bigint` values.
	 */
	declare readonly uid: [bigint, bigint];

	/**
	 * The human readable nickname of the profile.
	 */
	declare readonly nickname: string;

	/**
	 * The raw JPEG data for the profile image. Can be decoded with the `Image` class.
	 */
	declare readonly image: ArrayBuffer;

	/**
	 * @ignore
	 */
	constructor() {
		assertInternalConstructor(arguments);
	}
}
$.accountProfileInit(Profile);

/**
 * Return a {@link Profile} instance if there was a preselected user
 * when launching the application, or `null` if there was none.
 */
export function currentProfile() {
	_init();
	const p = $.accountCurrentProfile();
	if (p) Object.setPrototypeOf(p, Profile.prototype);
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
	if (p) Object.setPrototypeOf(p, Profile.prototype);
	return p;
}

/**
 * Can be used as an iterator to retrieve the list of user profiles.
 *
 * @example
 *
 * ```typescript
 * for (const profile of Switch.profiles) {
 *   console.log(profile.nickname);
 * }
 * ```
 */
export const profiles = {
	*[Symbol.iterator]() {
		_init();
		const pr = $.accountProfiles();
		for (const p of pr) {
			Object.setPrototypeOf(p, Profile.prototype);
			yield p;
		}
	},
};
