import { $ } from '../$';
import { proto, stub } from '../utils';

export interface ServiceDispatchParams {
	//Handle target_session;
	//u32 context;

	//SfBufferAttrs buffer_attrs;
	//SfBuffer buffers[8];
	bufferAttrs: number[];
	buffers: ArrayBuffer[];

	//bool in_send_pid;

	//u32 in_num_objects;
	//const Service* in_objects[8];

	//u32 in_num_handles;
	//Handle in_handles[8];

	//u32 out_num_objects;
	//Service* out_objects;

	//SfOutHandleAttrs out_handle_attrs;
	//Handle* out_handles;
}

export class Service {
	constructor(name: string) {
		return proto($.serviceNew(name), Service);
	}

	isActive() {
		stub();
	}

	isOverride() {
		stub();
	}

	dispatch(rid: number, dispatchParams?: ServiceDispatchParams) {
		return this.dispatchInOut(rid, undefined, undefined, dispatchParams);
	}

	dispatchIn(
		rid: number,
		inData: ArrayBuffer,
		dispatchParams?: ServiceDispatchParams,
	) {
		return this.dispatchInOut(rid, inData, undefined, dispatchParams);
	}

	dispatchOut(
		rid: number,
		outData: ArrayBuffer,
		dispatchParams?: ServiceDispatchParams,
	) {
		return this.dispatchInOut(rid, undefined, outData, dispatchParams);
	}

	dispatchInOut(
		rid: number,
		inData?: ArrayBuffer,
		outData?: ArrayBuffer,
		dispatchParams?: ServiceDispatchParams,
	) {
		stub();
	}
}
$.serviceInit(Service);
