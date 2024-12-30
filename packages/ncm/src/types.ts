import { ArrayBufferStruct, createSingleton, u8, view } from '@nx.js/util';

const contentIdCache = createSingleton(
	(self: NcmContentInfo) => new NcmContentId(self, 0x0),
);
const contentMetaKeyCache = createSingleton(
	(self: NcmContentStorageRecord) => new NcmContentMetaKey(self, 0x0),
);

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
export class NcmContentId extends ArrayBufferStruct {
	//u8 c[0x10]; ///< Id
	static sizeof = 0x10 as const;

	toString() {
		if (this.byteLength !== 0x10) {
			throw new Error('Content ID must be 16 bytes');
		}
		const v = view(this);
		return `${v.getBigUint64(0).toString(16).padStart(16, '0')}${v
			.getBigUint64(8)
			.toString(16)
			.padStart(16, '0')}`;
	}

	static from(s: string) {
		const lower = BigInt(`0x${s.slice(0, 16)}`);
		const upper = BigInt(`0x${s.slice(16, 32)}`);
		const id = new NcmContentId();
		const v = view(id);
		v.setBigUint64(0, lower);
		v.setBigUint64(8, upper);
		return id;
	}
}

/// PlaceHolderId
export class NcmPlaceHolderId extends ArrayBufferStruct {
	//Uuid uuid;  ///< UUID
	static sizeof = 0x10 as const;
}

/// ContentStorageRecord
export class NcmContentStorageRecord extends ArrayBufferStruct {
	//NcmContentMetaKey key;
	//u8 storage_id;
	//u8 padding[0x7];
	static sizeof = 0x18 as const;

	get key() {
		return contentMetaKeyCache(this);
	}
	set key(v: NcmContentMetaKey) {
		u8(this).set(u8(v), 0);
	}

	get storageId(): NcmStorageId {
		return view(this).getUint8(0x10);
	}
	set storageId(v: NcmStorageId) {
		view(this).setUint8(0x10, v);
	}
}

/// ContentMetaKey
export class NcmContentMetaKey extends ArrayBufferStruct {
	//u64 id;                             ///< Id.
	//u32 version;                        ///< Version.
	//u8 type;                            ///< \ref NcmContentMetaType
	//u8 install_type;                    ///< \ref NcmContentInstallType
	//u8 padding[2];                      ///< Padding.
	static sizeof = 0x10 as const;

	get id() {
		return view(this).getBigUint64(0x0, true);
	}
	set id(v: bigint) {
		view(this).setBigUint64(0x0, v, true);
	}

	get version() {
		return view(this).getUint32(0x8, true);
	}
	set version(v: number) {
		view(this).setUint32(0x8, v, true);
	}

	get type(): NcmContentMetaType {
		return view(this).getUint8(0xc);
	}
	set type(v: NcmContentMetaType) {
		view(this).setUint8(0xc, v);
	}

	get installType(): NcmContentInstallType {
		return view(this).getUint8(0xd);
	}
	set installType(v: NcmContentInstallType) {
		view(this).setUint8(0xd, v);
	}
}

/// ContentInfo
export class NcmContentInfo extends ArrayBufferStruct {
	//NcmContentId content_id;     ///< \ref NcmContentId
	//u32 size_low;                ///< Content size (low).
	//u8 size_high;                ///< Content size (high).
	//u8 attr;                     ///< Content attributes.
	//u8 content_type;             ///< \ref NcmContentType.
	//u8 id_offset;                ///< Offset of this content. Unused by most applications.
	static sizeof = 0x18 as const;

	get contentId() {
		return contentIdCache(this);
	}
	set contentId(v: NcmContentId) {
		u8(this).set(u8(v), 0);
	}

	get sizeLow() {
		return view(this).getUint32(0x10, true);
	}
	get sizeHigh() {
		return view(this).getUint8(0x14);
	}
	get size(): bigint {
		return (BigInt(this.sizeHigh) << 32n) | BigInt(this.sizeLow);
	}
	set size(v: bigint) {
		const dv = view(this);
		dv.setUint32(0x10, Number(v & 0xffffffffn), true);
		dv.setUint8(0x14, Number(v >> 32n));
	}

	get attr() {
		return view(this).getUint8(0x15);
	}
	set attr(v: number) {
		view(this).setUint8(0x15, v);
	}

	get contentType(): NcmContentType {
		return view(this).getUint8(0x16);
	}
	set contentType(v: NcmContentType) {
		view(this).setUint8(0x16, v);
	}

	get idOffset() {
		return view(this).getUint8(0x17);
	}
	set idOffset(v: number) {
		view(this).setUint8(0x17, v);
	}
}

/// ContentMetaHeader
export class NcmContentMetaHeader extends ArrayBufferStruct {
	// u16 extended_header_size;           ///< Size of optional struct that comes after this one.
	// u16 content_count;                  ///< Number of NcmContentInfos after the extra bytes.
	// u16 content_meta_count;             ///< Number of NcmContentMetaInfos that come after the NcmContentInfos.
	// u8 attributes;                      ///< Usually None (0).
	// u8 storage_id;                      ///< Usually None (0).
	static sizeof = 0x8 as const;

	get extendedHeaderSize() {
		return view(this).getUint16(0x0, true);
	}
	set extendedHeaderSize(v: number) {
		view(this).setUint16(0x0, v, true);
	}

	get contentCount() {
		return view(this).getUint16(0x2, true);
	}
	set contentCount(v: number) {
		view(this).setUint16(0x2, v, true);
	}

	get contentMetaCount() {
		return view(this).getUint16(0x4, true);
	}
	set contentMetaCount(v: number) {
		view(this).setUint16(0x4, v, true);
	}

	get attributes() {
		return view(this).getUint8(0x6);
	}
	set attributes(v: number) {
		view(this).setUint8(0x6, v);
	}

	get storageId() {
		return view(this).getUint8(0x7);
	}
	set storageId(v: number) {
		view(this).setUint8(0x7, v);
	}
}

/// ApplicationMetaExtendedHeader
export class NcmApplicationMetaExtendedHeader extends ArrayBufferStruct {
	// u64 patch_id;                     ///< PatchId of this application's patch.
	// u32 required_system_version;      ///< Firmware version required by this application.
	// u32 required_application_version; ///< [9.0.0+] Owner application version required by this application. Previously padding.
	static sizeof = 0x10 as const;

	get patchId() {
		return view(this).getBigUint64(0x0, true);
	}
	set patchId(v: bigint) {
		view(this).setBigUint64(0x0, v, true);
	}

	get requiredSystemVersion() {
		return view(this).getUint32(0x8, true);
	}
	set requiredSystemVersion(v: number) {
		view(this).setUint32(0x8, v, true);
	}

	get requiredApplicationVersion() {
		return view(this).getUint32(0xc, true);
	}
	set requiredApplicationVersion(v: number) {
		view(this).setUint32(0xc, v, true);
	}
}

/// PatchMetaExtendedHeader
export class NcmPatchMetaExtendedHeader extends ArrayBufferStruct {
	// u64 application_id;          ///< ApplicationId of this patch's corresponding application.
	// u32 required_system_version; ///< Firmware version required by this patch.
	// u32 extended_data_size;      ///< Size of the extended data following the NcmContentInfos.
	// u8 reserved[0x8];            ///< Unused.
	static sizeof = 0x18 as const;

	get applicationId() {
		return view(this).getBigUint64(0x0, true);
	}
	set applicationId(v: bigint) {
		view(this).setBigUint64(0x0, v, true);
	}

	get requiredSystemVersion() {
		return view(this).getUint32(0x8, true);
	}
	set requiredSystemVersion(v: number) {
		view(this).setUint32(0x8, v, true);
	}

	get extendedDataSize() {
		return view(this).getUint32(0xc, true);
	}
	set extendedDataSize(v: number) {
		view(this).setUint32(0xc, v, true);
	}
}
