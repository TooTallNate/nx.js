/// StorageId
export enum NcmStorageId {
	None = 0, ///< None
	Host = 1, ///< Host
	GameCard = 2, ///< GameCard
	BuiltInSystem = 3, ///< BuiltInSystem
	BuiltInUser = 4, ///< BuiltInUser
	SdCard = 5, ///< SdCard
	Any = 6, ///< Any
}

/// ContentType
export enum NcmContentType {
	Meta = 0, ///< Meta
	Program = 1, ///< Program
	Data = 2, ///< Data
	Control = 3, ///< Control
	HtmlDocument = 4, ///< HtmlDocument
	LegalInformation = 5, ///< LegalInformation
	DeltaFragment = 6, ///< DeltaFragment
}

/// ContentMetaType
export enum NcmContentMetaType {
	Unknown = 0x0, ///< Unknown
	SystemProgram = 0x1, ///< SystemProgram
	SystemData = 0x2, ///< SystemData
	SystemUpdate = 0x3, ///< SystemUpdate
	BootImagePackage = 0x4, ///< BootImagePackage
	BootImagePackageSafe = 0x5, ///< BootImagePackageSafe
	Application = 0x80, ///< Application
	Patch = 0x81, ///< Patch
	AddOnContent = 0x82, ///< AddOnContent
	Delta = 0x83, ///< Delta
	DataPatch = 0x84, ///< DataPatch
}
NcmContentMetaType;

/// ContentMetaAttribute
export enum NcmContentMetaAttribute {
	None = 0, ///< None
	IncludesExFatDriver = 1 << 0, ///< IncludesExFatDriver
	Rebootless = 1 << 1, ///< Rebootless
	Compacted = 1 << 2, ///< Compacted
}

/// ContentInstallType
export enum NcmContentInstallType {
	Full = 0, ///< Full
	FragmentOnly = 1, ///< FragmentOnly
	Unknown = 7, ///< Unknown
}

/// ContentMetaPlatform
export enum NcmContentMetaPlatform {
	Nx = 0, ///< Nx
}

/// ContentId
//typedef struct {
//    u8 c[0x10]; ///< Id
//} NcmContentId;
declare const contentIdSymbol: unique symbol;
export type NcmContentId = ArrayBuffer & {
	[contentIdSymbol]: void;
};

/// PlaceHolderId
//typedef struct {
//    Uuid uuid;  ///< UUID
//} NcmPlaceHolderId;
declare const placeHolderIdSymbol: unique symbol;
export type NcmPlaceHolderId = ArrayBuffer & {
	[placeHolderIdSymbol]: void;
};

/// ContentMetaKey
//typedef struct {
//    u64 id;                             ///< Id.
//    u32 version;                        ///< Version.
//    u8 type;                            ///< \ref NcmContentMetaType
//    u8 install_type;                    ///< \ref NcmContentInstallType
//    u8 padding[2];                      ///< Padding.
//} NcmContentMetaKey;
export type NcmContentMetaKey = ArrayBuffer;
