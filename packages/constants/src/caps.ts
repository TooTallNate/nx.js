export enum CapsAlbumStorage {
	Nand = 0, ///< Nand
	Sd = 1, ///< Sd
}

export enum CapsAlbumFileContents {
	ScreenShot = 0,
	Movie = 1,
	ExtraScreenShot = 2,
	ExtraMovie = 3,
}

export enum CapsAlbumFileContentsFlag {
	ScreenShot = 1 << 0, ///< Query for ScreenShot files.
	Movie = 1 << 1, ///< Query for Movie files.
}
