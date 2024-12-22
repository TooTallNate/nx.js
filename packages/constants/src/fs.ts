export enum FsSaveDataSpaceId {
	System = 0, ///< System
	User = 1, ///< User
	SdSystem = 2, ///< SdSystem
	Temporary = 3, ///< [3.0.0+] Temporary
	SdUser = 4, ///< [4.0.0+] SdUser
	ProperSystem = 100, ///< [3.0.0+] ProperSystem
	SafeMode = 101, ///< [3.0.0+] SafeMode
}

export enum FsSaveDataType {
	System = 0, ///< System
	Account = 1, ///< Account
	Bcat = 2, ///< Bcat
	Device = 3, ///< Device
	Temporary = 4, ///< [3.0.0+] Temporary
	Cache = 5, ///< [3.0.0+] Cache
	SystemBcat = 6, ///< [4.0.0+] SystemBcat
}

export enum BisPartitionId {
	BootPartition1Root = 0,

	BootPartition2Root = 10,

	UserDataRoot = 20,
	BootConfigAndPackage2Part1 = 21,
	BootConfigAndPackage2Part2 = 22,
	BootConfigAndPackage2Part3 = 23,
	BootConfigAndPackage2Part4 = 24,
	BootConfigAndPackage2Part5 = 25,
	BootConfigAndPackage2Part6 = 26,
	CalibrationBinary = 27,
	CalibrationFile = 28,
	SafeMode = 29,
	User = 30,
	System = 31,
	SystemProperEncryption = 32,
	SystemProperPartition = 33,
	SignedSystemPartitionOnSafeMode = 34,
	DeviceTreeBlob = 35,
	System0 = 36,
}

export enum FsFileSystemType {
	Logo = 2, ///< Logo
	ContentControl = 3, ///< ContentControl
	ContentManual = 4, ///< ContentManual
	ContentMeta = 5, ///< ContentMeta
	ContentData = 6, ///< ContentData
	ApplicationPackage = 7, ///< ApplicationPackage
	RegisteredUpdate = 8, ///< [4.0.0+] RegisteredUpdate
}

export enum FsContentAttributes {
	None = 0x0,
	All = 0xf,
}
