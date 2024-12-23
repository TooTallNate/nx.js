import { SfBufferAttr } from '@nx.js/constants';
import { ncm } from './service';
import {
	NcmContentId,
	NcmContentMetaKey,
	NcmContentType,
	NcmStorageId,
} from './types';
import { u8, view } from '@nx.js/util';

export class NcmContentMetaDatabase {
	static open(storageId: NcmStorageId) {
		//Result ncmOpenContentMetaDatabase(NcmContentMetaDatabase* out_content_meta_database, NcmStorageId storage_id) {
		//  return serviceDispatchIn(&g_ncmSrv, 5, storage_id,
		//     .out_num_objects = 1,
		//     .out_objects = out_content_meta_database,
		//	);
		//}
		const out = new Switch.Service();
		const inArr = new Uint8Array([storageId]);
		ncm.dispatchIn(5, inArr, {
			outObjects: [out],
		});
		return new NcmContentMetaDatabase(out);
	}

	#srv: Switch.Service;

	/**
	 * @private
	 */
	constructor(srv: Switch.Service) {
		this.#srv = srv;
	}

	set(key: NcmContentMetaKey, data: ArrayBuffer | ArrayBufferView) {
		//Result ncmContentMetaDatabaseSet(NcmContentMetaDatabase* db, const NcmContentMetaKey* key, const void* data, u64 data_size) {
		//    return serviceDispatchIn(&db->s, 0, *key,
		//        .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_In },
		//        .buffers = { { data, data_size } },
		//    );
		//}
		this.#srv.dispatchIn(0, key, {
			bufferAttrs: [SfBufferAttr.HipcMapAlias | SfBufferAttr.In],
			buffers: [data],
		});
	}

	getInto(key: NcmContentMetaKey, data: ArrayBufferView): bigint {
		//Result ncmContentMetaDatabaseGet(NcmContentMetaDatabase* db, const NcmContentMetaKey* key, u64* out_size, void* out_data, u64 out_data_size) {
		//    return serviceDispatchInOut(&db->s, 1, *key, *out_size,
		//        .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_Out },
		//        .buffers = { { out_data, out_data_size } },
		//    );
		//}
		const out = new BigUint64Array(1);
		this.#srv.dispatchInOut(1, key, out.buffer, {
			bufferAttrs: [SfBufferAttr.HipcMapAlias | SfBufferAttr.Out],
			buffers: [data],
		});
		return out[0];
	}

	get(key: NcmContentMetaKey): ArrayBufferView {
		const expectedSize = this.getSize(key);
		const data = new Uint8Array(Number(expectedSize));
		const size = this.getInto(key, data);
		if (size !== expectedSize) {
			throw new Error(`Unexpected size: ${size} (expected: ${expectedSize})`);
		}
		return data;
	}

	delete(key: NcmContentMetaKey) {
		//Result ncmContentMetaDatabaseRemove(NcmContentMetaDatabase* db, const NcmContentMetaKey *key) {
		//    return serviceDispatchIn(&db->s, 2, *key);
		//}
		this.#srv.dispatchIn(2, key);
	}

	getContentIdByType(
		key: NcmContentMetaKey,
		type: NcmContentType,
	): NcmContentId {
		//Result ncmContentMetaDatabaseGetContentIdByType(NcmContentMetaDatabase* db, NcmContentId* out_content_id, const NcmContentMetaKey* key, NcmContentType type) {
		//    const struct {
		//        u8 type;
		//        u8 padding[7];
		//        NcmContentMetaKey key;
		//    } in = { type, {0}, *key };
		//    return serviceDispatchInOut(&db->s, 3, in, *out_content_id);
		//}
		const inData = new Uint8Array(0x18);
		inData[0] = type;
		inData.set(u8(key), 0x8);
		const out = new NcmContentId();
		this.#srv.dispatchInOut(3, inData, out);
		return out;
	}

	listContentInfo(
		key: NcmContentMetaKey,
		startIndex: number,
		outInfo: ArrayBuffer,
	) {
		//Result ncmContentMetaDatabaseListContentInfo(NcmContentMetaDatabase* db, s32* out_entries_written, NcmContentInfo* out_info, s32 count, const NcmContentMetaKey* key, s32 start_index) {
		//	const struct {
		//		s32 start_index;
		//		u32 pad;
		//		NcmContentMetaKey key;
		//	} in = { start_index, 0, *key };
		//	return serviceDispatchInOut(&db->s, 4, in, *out_entries_written,
		//		.buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_Out },
		//		.buffers = { { out_info, count*sizeof(NcmContentInfo) } },
		//	);
		//}
		const inData = new Uint8Array(0x18);
		const inView = view(inData);
		inView.setInt32(0, startIndex ?? 0, true);
		inView.setUint32(4, 0, true);
		inData.set(u8(key), 0x8);
		const out = new Int32Array(1);
		this.#srv.dispatchInOut(4, inData, out, {
			bufferAttrs: [SfBufferAttr.HipcMapAlias | SfBufferAttr.Out],
			buffers: [outInfo],
		});
		return out[0];
	}

	has(key: NcmContentMetaKey): boolean {
		//Result ncmContentMetaDatabaseHas(NcmContentMetaDatabase* db, bool* out, const NcmContentMetaKey* key) {
		//    u8 tmp=0;
		//    Result rc = serviceDispatchInOut(&db->s, 8, *key, tmp);
		//    if (R_SUCCEEDED(rc) && out) *out = tmp & 1;
		//    return rc;
		//}
		const out = new Uint8Array(1);
		this.#srv.dispatchInOut(8, key, out);
		return Boolean(out[0] & 1);
	}

	hasAll(keys: NcmContentMetaKey[]): boolean {
		//Result ncmContentMetaDatabaseHasAll(NcmContentMetaDatabase* db, bool* out, const NcmContentMetaKey* keys, s32 count) {
		//    u8 tmp=0;
		//    Result rc = serviceDispatchOut(&db->s, 9, *out,
		//        .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_In },
		//        .buffers = { { keys, count*sizeof(NcmContentMetaKey) } },
		//    );
		//    if (R_SUCCEEDED(rc) && out) *out = tmp & 1;
		//    return rc;
		//}
		const inData = new Uint8Array(keys.length * NcmContentMetaKey.sizeof);
		for (let i = 0; i < keys.length; i++) {
			inData.set(u8(keys[i]), i * 0x10);
		}
		const out = new Uint8Array(1);
		this.#srv.dispatchOut(9, out, {
			bufferAttrs: [SfBufferAttr.HipcMapAlias | SfBufferAttr.In],
			buffers: [inData],
		});
		return Boolean(out[0] & 1);
	}

	getSize(key: NcmContentMetaKey): bigint {
		//Result ncmContentMetaDatabaseGetSize(NcmContentMetaDatabase* db, u64* out_size, const NcmContentMetaKey* key) {
		//    return serviceDispatchInOut(&db->s, 10, *key, *out_size);
		//}
		const out = new BigUint64Array(1);
		this.#srv.dispatchInOut(10, key, out);
		return out[0];
	}

	commit() {
		//Result ncmContentMetaDatabaseCommit(NcmContentMetaDatabase* db) {
		//    return _ncmCmdNoIO(&db->s, 15);
		//}
		this.#srv.dispatch(15);
	}
}
