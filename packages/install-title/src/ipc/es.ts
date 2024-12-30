// Ported from `tinwoo/nx/ipc/es.c`
import { SfBufferAttr } from '@nx.js/constants';

const es = new Switch.Service('es');

export function esImportTicket(tik: ArrayBuffer, cert: ArrayBuffer) {
	//Result esImportTicket(void const *tikBuf, size_t tikSize, void const *certBuf, size_t certSize) {
	//    return serviceDispatch(&g_esSrv, 1,
	//        .buffer_attrs = {
	//            SfBufferAttr_HipcMapAlias | SfBufferAttr_In,
	//            SfBufferAttr_HipcMapAlias | SfBufferAttr_In,
	//        },
	//        .buffers = {
	//            { tikBuf,   tikSize },
	//            { certBuf,  certSize },
	//        },
	//    );
	//}
	es.dispatch(1, {
		bufferAttrs: [
			SfBufferAttr.HipcMapAlias | SfBufferAttr.In,
			SfBufferAttr.HipcMapAlias | SfBufferAttr.In,
		],
		buffers: [tik, cert],
	});
}
