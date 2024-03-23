/// Type of keyboard.
export enum SwkbdType {
	Normal = 0, ///< Normal keyboard.
	NumPad = 1, ///< Number pad. The buttons at the bottom left/right are only available when they're set by \ref swkbdConfigSetLeftOptionalSymbolKey / \ref swkbdConfigSetRightOptionalSymbolKey.
	QWERTY = 2, ///< QWERTY (and variants) keyboard only.
	Unknown3 = 3, ///< The same as SwkbdType_Normal keyboard.
	Latin = 4, ///< All Latin like languages keyboard only (without CJK keyboard).
	ZhHans = 5, ///< Chinese Simplified keyboard only.
	ZhHant = 6, ///< Chinese Traditional keyboard only.
	Korean = 7, ///< Korean keyboard only.
	All = 8, ///< All language keyboards.
	Unknown9 = 9, ///< Unknown
}
