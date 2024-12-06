import { $ } from '../$';
import { proto, stub } from '../utils';

export interface ServiceDispatchParams {
	//Handle target_session;
	targetSession?: number;

	//u32 context;
	context?: number;

	//SfBufferAttrs buffer_attrs;
	//SfBuffer buffers[8];
	bufferAttrs?: number[];
	buffers?: ArrayBuffer[];

	//bool in_send_pid;
	inSendPid?: boolean;

	//u32 in_num_objects;
	//const Service* in_objects[8];
	inObjects?: Service[];

	//u32 in_num_handles;
	//Handle in_handles[8];
	inHandles?: number[];

	//u32 out_num_objects;
	//Service* out_objects;
	// XXX: This seems to always be 1 (hence why its not an array?)
	outObjects?: Service[];

	//SfOutHandleAttrs out_handle_attrs;
	//Handle* out_handles;
	outHandleAttrs?: number[];
	outHandles?: number[];
}

export class Service {
	constructor(name?: string) {
		return proto($.serviceNew(name), Service);
	}

	isActive() {
		stub();
	}

	isOverride() {
		stub();
	}

	dispatch(rid: number, params?: ServiceDispatchParams) {
		this.dispatchInOut(rid, undefined, undefined, params);
	}

	dispatchIn(rid: number, inData: ArrayBuffer, parmas?: ServiceDispatchParams) {
		this.dispatchInOut(rid, inData, undefined, parmas);
	}

	dispatchOut(
		rid: number,
		outData: ArrayBuffer,
		params?: ServiceDispatchParams,
	) {
		this.dispatchInOut(rid, undefined, outData, params);
	}

	dispatchInOut(
		rid: number,
		inData?: ArrayBuffer,
		outData?: ArrayBuffer,
		params?: ServiceDispatchParams,
	) {
		stub();
	}
}
$.serviceInit(Service);
