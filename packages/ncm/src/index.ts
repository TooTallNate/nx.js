import { ncm } from './service';
import type { NcmStorageId } from './types';

export * from './types';
export * from './content-storage';
export * from './content-meta-database';

export function createContentStorage(storageId: NcmStorageId) {
	//Result ncmCreateContentStorage(NcmStorageId storage_id) {
	//    return _ncmCmdInU8(&g_ncmSrv, storage_id, 0);
	//}
	const inData = new Uint8Array([storageId]);
	ncm.dispatchIn(0, inData.buffer);
}

export function createContentMetaDatabase(storageId: NcmStorageId) {
	//Result ncmCreateContentMetaDatabase(NcmStorageId storage_id) {
	//    return _ncmCmdInU8(&g_ncmSrv, storage_id, 1);
	//}
	const inData = new Uint8Array([storageId]);
	ncm.dispatchIn(1, inData.buffer);
}

export function verifyContentStorage(storageId: NcmStorageId) {
	//Result ncmVerifyContentStorage(NcmStorageId storage_id) {
	//    return _ncmCmdInU8(&g_ncmSrv, storage_id, 2);
	//}
	const inData = new Uint8Array([storageId]);
	ncm.dispatchIn(2, inData.buffer);
}

export function verifyContentMetaDatabase(storageId: NcmStorageId) {
	//Result ncmVerifyContentMetaDatabase(NcmStorageId storage_id) {
	//    return _ncmCmdInU8(&g_ncmSrv, storage_id, 3);
	//}
	const inData = new Uint8Array([storageId]);
	ncm.dispatchIn(3, inData.buffer);
}
