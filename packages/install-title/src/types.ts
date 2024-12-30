import { NcmContentInfo, NcmContentMetaType } from '@nx.js/ncm';
import { ArrayBufferStruct, view, u8 } from '@nx.js/util';

export class PackagedContentMetaHeader extends ArrayBufferStruct {
	//u64 title_id;
	//u32 version;
	//u8 type; /* NcmContentMetaType */
	//u8 _0xd;
	//u16 extended_header_size;
	//u16 content_count;
	//u16 content_meta_count;
	//u8 attributes;
	//u8 storage_id;
	//u8 install_type; /* NcmContentInstallType */
	//bool comitted;
	//u32 required_system_version;
	//u32 _0x1c;
	static sizeof = 0x20 as const;

	get titleId() {
		return view(this).getBigUint64(0x0, true);
	}
	get version() {
		return view(this).getUint32(0x8, true);
	}
	get type(): NcmContentMetaType {
		return view(this).getUint8(0xc);
	}
	get extendedHeaderSize() {
		return view(this).getUint16(0xe, true);
	}
	get contentCount() {
		return view(this).getUint16(0x10, true);
	}
}

export class PackagedContentInfo extends ArrayBufferStruct {
	//u8 hash[0x20];
	//NcmContentInfo content_info;
	static sizeof = 0x38 as const;

	get hash() {
		return u8(this).subarray(0x0, 0x20);
	}
	get contentInfo() {
		return new NcmContentInfo(this, 0x20);
	}
}
