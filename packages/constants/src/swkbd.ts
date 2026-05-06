/**
 * Selects which on-screen software keyboard layout is shown to the user.
 */
export enum SwkbdType {
	/** Normal keyboard with the user's current language layout. */
	Normal = 0,
	/**
	 * Number pad. The buttons at the bottom left/right are only available
	 * when they are set with `swkbdConfigSetLeftOptionalSymbolKey` /
	 * `swkbdConfigSetRightOptionalSymbolKey`.
	 */
	NumPad = 1,
	/** QWERTY (and regional variants) keyboard only. */
	QWERTY = 2,
	/** Behaves the same as `Normal`. */
	Unknown3 = 3,
	/** All Latin-like languages — no CJK input. */
	Latin = 4,
	/** Chinese (Simplified) keyboard only. */
	ZhHans = 5,
	/** Chinese (Traditional) keyboard only. */
	ZhHant = 6,
	/** Korean keyboard only. */
	Korean = 7,
	/** All language keyboards available to the user. */
	All = 8,
	/** Unknown layout reserved by the OS. */
	Unknown9 = 9,
}
