/**
 * Storage backend for an album entry.
 */
export enum CapsAlbumStorage {
	/** Stored on internal NAND. */
	Nand = 0,
	/** Stored on the SD card. */
	Sd = 1,
}

/**
 * The kind of media represented by an album file.
 */
export enum CapsAlbumFileContents {
	/** Standard screenshot (JPEG image). */
	ScreenShot = 0,
	/** Standard captured movie (MP4 video). */
	Movie = 1,
	/** "Extra" screenshot (e.g. from supplementary capture sources). */
	ExtraScreenShot = 2,
	/** "Extra" captured movie. */
	ExtraMovie = 3,
}

/**
 * Bitmask used when querying the album for files of specific types.
 */
export enum CapsAlbumFileContentsFlag {
	/** Include screenshot files in the query. */
	ScreenShot = 1 << 0,
	/** Include movie files in the query. */
	Movie = 1 << 1,
}
