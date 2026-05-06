/**
 * Identifies which save data space (storage backend) a save data store
 * lives in.
 *
 * @see {@link FsSaveDataType}
 */
export enum FsSaveDataSpaceId {
	/** Internal NAND, system saves. */
	System = 0,
	/** Internal NAND, application user saves (the most common space for game saves). */
	User = 1,
	/** SD card, system saves. */
	SdSystem = 2,
	/** Internal NAND, temporary save data. Available since system version 3.0.0. */
	Temporary = 3,
	/** SD card, application user saves. Available since system version 4.0.0. */
	SdUser = 4,
	/** Internal NAND, "proper" system saves. Available since system version 3.0.0. */
	ProperSystem = 100,
	/** Safe-mode partition system saves. Available since system version 3.0.0. */
	SafeMode = 101,
}

/**
 * Identifies what kind of save data a [`Switch.SaveData`](/runtime/api/namespaces/Switch/classes/SaveData)
 * store represents. Used to filter the iterator on `Switch.SaveData` and to
 * compare against [`SaveData.type`](/runtime/api/namespaces/Switch/classes/SaveData#type).
 */
export enum FsSaveDataType {
	/** System save data, owned by the OS or a system module. */
	System = 0,
	/** Per-user account save data — the typical kind for application save files. */
	Account = 1,
	/** BCAT (broadcast) save data, used for downloaded promotional/online content. */
	Bcat = 2,
	/** Device save data, shared across all user accounts on the console. */
	Device = 3,
	/** Temporary save data that is wiped on reboot. Available since system version 3.0.0. */
	Temporary = 4,
	/** Cache save data, used for non-critical caches that the OS may evict. Available since system version 3.0.0. */
	Cache = 5,
	/** System BCAT save data. Available since system version 4.0.0. */
	SystemBcat = 6,
}

/**
 * Identifies a partition on the BIS (Built-in Storage, the internal eMMC).
 * Used by low-level BIS APIs.
 */
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

/**
 * Identifies a content filesystem inside an installed title.
 */
export enum FsFileSystemType {
	/** Logo partition. */
	Logo = 2,
	/** Control NCA — contains the title's control data (icons, NACP, etc.). */
	ContentControl = 3,
	/** Manual NCA — contains the bundled HTML manual. */
	ContentManual = 4,
	/** Meta NCA — contains content metadata. */
	ContentMeta = 5,
	/** Data NCA — contains the title's main data (RomFS). */
	ContentData = 6,
	/** Application package (installed NSP/XCI). */
	ApplicationPackage = 7,
	/** Registered update partition. Available since system version 4.0.0. */
	RegisteredUpdate = 8,
}

/**
 * Bitmask of attributes for content filesystem operations.
 */
export enum FsContentAttributes {
	/** No special attributes. */
	None = 0x0,
	/** All attributes set (used as a mask). */
	All = 0xf,
}
