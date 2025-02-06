// Ported from `tinwoo/nx/ipc/ns_ext.c`
import { SfBufferAttr } from '@nx.js/constants';
import { NsApplicationRecordType } from './types';

export * from './types';

const nsAm2 = new Switch.Service('ns:am2');
const nsAppManSrv = new Switch.Service();
nsAm2.dispatch(7996, {
	outObjects: [nsAppManSrv],
});

export function nsPushApplicationRecord(
	titleId: bigint,
	lastModifiedEvent: NsApplicationRecordType,
	contentRecords: ArrayBuffer,
) {
	//Result nsPushApplicationRecord(u64 title_id, u8 last_modified_event, ContentStorageRecord *content_records_buf, size_t buf_size) {
	//    struct {
	//        u8 last_modified_event;
	//        u8 padding[0x7];
	//        u64 title_id;
	//    } in = { last_modified_event, {0}, title_id };
	//
	//    return serviceDispatchIn(&g_nsAppManSrv, 16, in,
	//        .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_In },
	//        .buffers = { { content_records_buf, buf_size } });
	//}
	const inData = new ArrayBuffer(0x10);
	new Uint8Array(inData)[0] = lastModifiedEvent;
	new BigUint64Array(inData, 0x8)[0] = titleId;
	nsAppManSrv.dispatchIn(16, inData, {
		bufferAttrs: [SfBufferAttr.HipcMapAlias | SfBufferAttr.In],
		buffers: [contentRecords],
	});
}

export function nsListApplicationRecordContentMeta(
	offset: bigint,
	titleId: bigint,
	outBuf: ArrayBuffer,
) {
	//Result nsListApplicationRecordContentMeta(u64 offset, u64 titleID, void *out_buf, size_t out_buf_size, u32 *entries_read_out) {
	//    struct {
	//        u64 offset;
	//        u64 titleID;
	//    } in = { offset, titleID };
	//
	//    struct {
	//        u32 entries_read;
	//    } out;
	//
	//    Result rc = serviceDispatchInOut(&g_nsAppManSrv, 17, in, out,
	//        .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_Out },
	//        .buffers = { { out_buf, out_buf_size } });
	//
	//    if (R_SUCCEEDED(rc) && entries_read_out) *entries_read_out = out.entries_read;
	//
	//    return rc;
	//}
	const inData = new BigUint64Array([offset, titleId]);
	const out = new Uint32Array(1);
	nsAppManSrv.dispatchInOut(17, inData, out, {
		bufferAttrs: [SfBufferAttr.HipcMapAlias | SfBufferAttr.Out],
		buffers: [outBuf],
	});
	return out[0];
}

export function nsDeleteApplicationRecord(titleId: bigint) {
	//Result nsDeleteApplicationRecord(u64 titleID) {
	//    struct {
	//        u64 titleID;
	//    } in = { titleID };
	//
	//    return serviceDispatchIn(&g_nsAppManSrv, 27, in);
	//}
	const inData = new BigUint64Array([titleId]);
	nsAppManSrv.dispatchIn(27, inData);
}

export function nsInvalidateApplicationControlCache(titleId: bigint) {
	//Result nsInvalidateApplicationControlCache(Service* srv, u64 tid) {
	//    return serviceDispatchIn(srv, 404, tid);
	//}
	const inData = new BigUint64Array([titleId]);
	nsAppManSrv.dispatchIn(404, inData);
}

export function nsIsAnyApplicationEntityInstalled(titleId: bigint) {
	//Result nsIsAnyApplicationEntityInstalled(u64 application_id, bool *out) {
	//	u8 tmp=0;
	//	if (R_SUCCEEDED(rc)) rc = serviceDispatchInOut(srv_ptr, 1300, application_id, tmp);
	//	if (R_SUCCEEDED(rc) && out) *out = tmp & 1;
	//}
	const inData = new BigUint64Array([titleId]);
	const out = new Uint8Array(1);
	nsAppManSrv.dispatchInOut(1300, inData, out);
	return Boolean(out[0] & 1);
}
