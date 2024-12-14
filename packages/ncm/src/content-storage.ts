import { SfBufferAttr } from '@nx.js/constants';
import { ncm } from './service';
import type { NcmContentId, NcmPlaceHolderId, NcmStorageId } from './types';

export class NcmContentStorage {
	static open(storageId: NcmStorageId) {
		//Result ncmOpenContentStorage(NcmContentStorage* out_content_storage, NcmStorageId storage_id) {
		//  return serviceDispatchIn(&g_ncmSrv, 4, storage_id,
		//     .out_num_objects = 1,
		//     .out_objects = out_content_storage,
		//	);
		//}
		const out = new Switch.Service();
		const inArr = new Uint8Array([storageId]);
		ncm.dispatchIn(4, inArr.buffer, {
			outObjects: [out],
		});
		return new NcmContentStorage(out);
	}

	#srv: Switch.Service;

	/**
	 * @private
	 */
	constructor(srv: Switch.Service) {
		this.#srv = srv;
	}

	generatePlaceHolderId(): NcmPlaceHolderId {
		//Result ncmContentStorageGeneratePlaceHolderId(NcmContentStorage* cs, NcmPlaceHolderId* out_id) {
		//  return serviceDispatchOut(&cs->s, 0, *out_id);
		//}
		const uuid = new ArrayBuffer(0x10);
		this.#srv.dispatchOut(0, uuid);
		return uuid;
	}

	createPlaceHolder(
		contentId: NcmContentId,
		placeholderId: NcmPlaceHolderId,
		size: bigint,
	) {
		//Result ncmContentStorageCreatePlaceHolder(NcmContentStorage* cs, const NcmContentId* content_id, const NcmPlaceHolderId* placeholder_id, s64 size) {
		//	if (hosversionBefore(16,0,0)) {
		//		const struct {
		//			NcmContentId content_id;
		//			NcmPlaceHolderId placeholder_id;
		//			s64 size;
		//		} in = { *content_id, *placeholder_id, size };
		//		return serviceDispatchIn(&cs->s, 1, in);
		//	} else {
		//		const struct {
		//			NcmPlaceHolderId placeholder_id;
		//			NcmContentId content_id;
		//			s64 size;
		//		} in = { *placeholder_id, *content_id, size };
		//		return serviceDispatchIn(&cs->s, 1, in);
		//	}
		//}
		const inData = new ArrayBuffer(0x28);
		const inDataView = new Uint8Array(inData);
		inDataView.set(new Uint8Array(contentId), 0);
		inDataView.set(new Uint8Array(placeholderId), 0x10);
		new BigInt64Array(inData, 0x20, 1)[0] = size;
		this.#srv.dispatchIn(1, inData);
	}

	deletePlaceHolder(placeholderId: NcmPlaceHolderId) {
		//Result ncmContentStorageDeletePlaceHolder(NcmContentStorage* cs, const NcmPlaceHolderId* placeholder_id) {
		//	return _ncmCmdInPlaceHolderId(&cs->s, placeholder_id, 2);
		//}
		this.#srv.dispatchIn(2, placeholderId);
	}

	hasPlaceHolder(placeholderId: NcmPlaceHolderId): boolean {
		//Result ncmContentStorageHasPlaceHolder(NcmContentStorage* cs, bool* out, const NcmPlaceHolderId* placeholder_id) {
		//	u8 tmp=0;
		//	Result rc = serviceDispatchInOut(&cs->s, 3, *placeholder_id, tmp);
		//	if (R_SUCCEEDED(rc) && out) *out = tmp & 1;
		//	return rc;
		//}
		const out = new Uint8Array(1);
		this.#srv.dispatchInOut(3, placeholderId, out.buffer);
		return Boolean(out[0] & 1);
	}

	writePlaceHolder(
		placeholderId: NcmPlaceHolderId,
		offset: bigint,
		data: ArrayBuffer,
	) {
		//Result ncmContentStorageWritePlaceHolder(NcmContentStorage* cs, const NcmPlaceHolderId* placeholder_id, u64 offset, const void* data, size_t data_size) {
		//	const struct {
		//		NcmPlaceHolderId placeholder_id;
		//		u64 offset;
		//	} in = { *placeholder_id, offset };
		//	return serviceDispatchIn(&cs->s, 4, in,
		//		.buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_In },
		//		.buffers = { { data, data_size } },
		//	);
		//}
		const inData = new ArrayBuffer(0x18);
		const inDataView = new Uint8Array(inData);
		inDataView.set(new Uint8Array(placeholderId), 0);
		new BigUint64Array(inData, 0x10, 1)[0] = offset;
		this.#srv.dispatchIn(4, inData, {
			bufferAttrs: [SfBufferAttr.HipcMapAlias | SfBufferAttr.In],
			buffers: [data],
		});
	}

	register(contentId: NcmContentId, placeholderId: NcmPlaceHolderId) {
		//Result ncmContentStorageRegister(NcmContentStorage* cs, const NcmContentId* content_id, const NcmPlaceHolderId* placeholder_id) {
		//	if (hosversionBefore(16,0,0)) {
		//		const struct {
		//			NcmContentId content_id;
		//			NcmPlaceHolderId placeholder_id;
		//		} in = { *content_id, *placeholder_id };
		//		return serviceDispatchIn(&cs->s, 5, in);
		//	} else {
		//		const struct {
		//			NcmPlaceHolderId placeholder_id;
		//			NcmContentId content_id;
		//		} in = { *placeholder_id, *content_id };
		//		return serviceDispatchIn(&cs->s, 5, in);
		//	}
		//}
		const inData = new ArrayBuffer(0x20);
		const inDataView = new Uint8Array(inData);
		inDataView.set(new Uint8Array(placeholderId), 0);
		inDataView.set(new Uint8Array(contentId), 0x10);
		this.#srv.dispatchIn(5, inData);
	}

	delete(contentId: NcmContentId) {
		//Result ncmContentStorageDelete(NcmContentStorage* cs, const NcmContentId* content_id) {
		//	return _ncmCmdInContentId(&cs->s, content_id, 6);
		//}
		this.#srv.dispatchIn(6, contentId);
	}

	has(contentId: NcmContentId): boolean {
		//Result ncmContentStorageHas(NcmContentStorage* cs, bool* out, const NcmContentId* content_id) {
		//	u8 tmp=0;
		//	Result rc = serviceDispatchInOut(&cs->s, 7, *content_id, tmp);
		//	if (R_SUCCEEDED(rc) && out) *out = tmp & 1;
		//	return rc;
		//}
		const out = new Uint8Array(1);
		this.#srv.dispatchInOut(7, contentId, out.buffer);
		return Boolean(out[0] & 1);
	}

	getPath(contentId: NcmContentId): string {
		//Result ncmContentStorageGetPath(NcmContentStorage* cs, char* out_path, size_t out_size, const NcmContentId* content_id) {
		//	char tmpbuf[0x300]={0};
		//	Result rc = serviceDispatchIn(&cs->s, 8, *content_id,
		//		.buffer_attrs = { SfBufferAttr_FixedSize | SfBufferAttr_HipcPointer | SfBufferAttr_Out },
		//		.buffers = { { tmpbuf, sizeof(tmpbuf) } },
		//	);
		//	if (R_SUCCEEDED(rc) && out_path) {
		//		strncpy(out_path, tmpbuf, out_size-1);
		//		out_path[out_size-1] = 0;
		//	}
		//	return rc;
		//}
		const out = new ArrayBuffer(0x300);
		this.#srv.dispatchIn(8, contentId, {
			bufferAttrs: [
				SfBufferAttr.FixedSize | SfBufferAttr.HipcPointer | SfBufferAttr.Out,
			],
			buffers: [out],
		});
		// TODO: null terminator
		return new TextDecoder().decode(new Uint8Array(out));
	}

	getPlaceHolderPath(placeholderId: NcmPlaceHolderId): string {
		//Result ncmContentStorageGetPlaceHolderPath(NcmContentStorage* cs, char* out_path, size_t out_size, const NcmPlaceHolderId* placeholder_id) {
		//	char tmpbuf[0x300]={0};
		//	Result rc = serviceDispatchIn(&cs->s, 9, *placeholder_id,
		//		.buffer_attrs = { SfBufferAttr_FixedSize | SfBufferAttr_HipcPointer | SfBufferAttr_Out },
		//		.buffers = { { tmpbuf, sizeof(tmpbuf) } },
		//	);
		//	if (R_SUCCEEDED(rc) && out_path) {
		//		strncpy(out_path, tmpbuf, out_size-1);
		//		out_path[out_size-1] = 0;
		//	}
		//	return rc;
		//}
		const out = new ArrayBuffer(0x300);
		this.#srv.dispatchIn(9, placeholderId, {
			bufferAttrs: [
				SfBufferAttr.FixedSize | SfBufferAttr.HipcPointer | SfBufferAttr.Out,
			],
			buffers: [out],
		});
		// TODO: null terminator
		return new TextDecoder().decode(new Uint8Array(out));
	}

	cleanupAllPlaceHolder() {
		//Result ncmContentStorageCleanupAllPlaceHolder(NcmContentStorage * cs) {
		//	return _ncmCmdNoIO(& cs -> s, 10);
		//}
		this.#srv.dispatch(10);
	}

	listPlaceHolder(maxCount = 20): NcmPlaceHolderId[] {
		//Result ncmContentStorageListPlaceHolder(NcmContentStorage* cs, NcmPlaceHolderId* out_ids, s32 count, s32* out_count) {
		//	return serviceDispatchOut(&cs->s, 11, *out_count,
		//		.buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_Out },
		//		.buffers = { { out_ids, count*sizeof(NcmPlaceHolderId) } },
		//	);
		//}
		const outIds = new ArrayBuffer(maxCount * 0x10);
		const out = new Int32Array(1);
		this.#srv.dispatchOut(11, out.buffer, {
			bufferAttrs: [SfBufferAttr.HipcMapAlias | SfBufferAttr.Out],
			buffers: [outIds],
		});
		const outCount = out[0];
		const rtn: NcmPlaceHolderId[] = new Array(outCount);
		for (let i = 0; i < outCount; i++) {
			rtn[i] = outIds.slice(i * 0x10, i * 0x10 + 0x10);
		}
		return rtn;
	}

	getContentCount(): number {
		//Result ncmContentStorageGetContentCount(NcmContentStorage* cs, s32* out_count) {
		//	return serviceDispatchOut(&cs->s, 12, *out_count);
		//}
		const out = new Int32Array(1);
		this.#srv.dispatchOut(12, out.buffer);
		return out[0];
	}

	listContentId(startOffset = 0, maxCount = 20): NcmContentId[] {
		//Result ncmContentStorageListContentId(NcmContentStorage* cs, NcmContentId* out_ids, s32 count, s32* out_count, s32 start_offset) {
		//	return serviceDispatchInOut(&cs->s, 13, start_offset, *out_count,
		//		.buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_Out },
		//		.buffers = { { out_ids, count*sizeof(NcmContentId) } },
		//	);
		//}
		const inData = new Int32Array([startOffset]);
		const outIds = new ArrayBuffer(maxCount * 0x10);
		const out = new Int32Array(1);
		this.#srv.dispatchInOut(13, inData.buffer, out.buffer, {
			bufferAttrs: [SfBufferAttr.HipcMapAlias | SfBufferAttr.Out],
			buffers: [outIds],
		});
		const outCount = out[0];
		const rtn: NcmContentId[] = new Array(outCount);
		for (let i = 0; i < outCount; i++) {
			rtn[i] = outIds.slice(i * 0x10, i * 0x10 + 0x10);
		}
		return rtn;
	}

	getSizeFromContentId(contentId: NcmContentId): bigint {
		//Result ncmContentStorageGetSizeFromContentId(NcmContentStorage* cs, s64* out_size, const NcmContentId* content_id) {
		//	return _ncmCmdInContentIdOutU64(&cs->s, content_id, (u64*)out_size, 14);
		//}
		const out = new BigInt64Array(1);
		this.#srv.dispatchInOut(14, contentId, out.buffer);
		return out[0];
	}

	disableForcibly() {
		//Result ncmContentStorageDisableForcibly(NcmContentStorage* cs) {
		//	return _ncmCmdNoIO(&cs->s, 15);
		//}
		this.#srv.dispatch(15);
	}

	get freeSpace(): bigint {
		//Result ncmContentStorageGetFreeSpaceSize(NcmContentStorage* cs, s64* out_size) {
		//	if (hosversionBefore(2,0,0)) return MAKERESULT(Module_Libnx, LibnxError_IncompatSysVer);
		//	return _ncmCmdNoInOutU64(&cs->s, (u64*)out_size, 22);
		//}
		const out = new BigInt64Array(1);
		this.#srv.dispatchOut(22, out.buffer);
		return out[0];
	}

	get totalSpace(): bigint {
		//Result ncmContentStorageGetTotalSpaceSize(NcmContentStorage* cs, s64* out_size) {
		//	if (hosversionBefore(2,0,0)) return MAKERESULT(Module_Libnx, LibnxError_IncompatSysVer);
		//	return _ncmCmdNoInOutU64(&cs->s, (u64*)out_size, 23);
		//}
		const out = new BigInt64Array(1);
		this.#srv.dispatchOut(23, out.buffer);
		return out[0];
	}

	flushPlaceHolder() {
		//Result ncmContentStorageFlushPlaceHolder(NcmContentStorage* cs) {
		//	if (hosversionBefore(3,0,0)) return MAKERESULT(Module_Libnx, LibnxError_IncompatSysVer);
		//	return _ncmCmdNoIO(&cs->s, 24);
		//}
		this.#srv.dispatch(24);
	}

	getSizeFromPlaceHolderId(placeholderId: NcmPlaceHolderId): bigint {
		//Result ncmContentStorageGetSizeFromPlaceHolderId(NcmContentStorage* cs, s64* out_size, const NcmPlaceHolderId* placeholder_id) {
		//	if (hosversionBefore(4,0,0)) return MAKERESULT(Module_Libnx, LibnxError_IncompatSysVer);
		//	return _ncmCmdInPlaceHolderIdOutU64(&cs->s, placeholder_id, (u64*)out_size, 25);
		//}
		const out = new BigInt64Array(1);
		this.#srv.dispatchInOut(25, placeholderId, out.buffer);
		return out[0];
	}

	repairInvalidFileAttribute() {
		//Result ncmContentStorageRepairInvalidFileAttribute(NcmContentStorage* cs) {
		//	if (hosversionBefore(4,0,0)) return MAKERESULT(Module_Libnx, LibnxError_IncompatSysVer);
		//	return _ncmCmdNoIO(&cs->s, 26);
		//}
		this.#srv.dispatch(26);
	}
}
