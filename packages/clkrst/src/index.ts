import { SfBufferAttr } from '@nx.js/constants';
import { PcvClockRatesListType, PcvModuleId } from './types';

export * from './types';

const clkrst = new Switch.Service('clkrst');

/**
 * Opens a session for the specified PCV module.
 *
 * @param moduleId The PCV module ID to open a session for.
 * @param unk Unknown value, defaults to `3` as seen in the wild.
 */
export function openSession(moduleId: PcvModuleId, unk = 3) {
	//Result clkrstOpenSession(ClkrstSession* session_out, PcvModuleId module_id, u32 unk) {
	//    const struct {
	//        u32 module_id;
	//        u32 unk;
	//    } in = { module_id, unk };
	//    return serviceDispatchIn(&g_clkrstSrv, 0, in,
	//        .out_num_objects = 1,
	//        .out_objects = &session_out->s,
	//    );
	//}
	const sessionSrv = new Switch.Service();
	const inArr = new Uint32Array([moduleId, unk]);
	clkrst.dispatchIn(0, inArr.buffer, {
		outObjects: [sessionSrv],
	});
	return new ClkrstSession(sessionSrv);
}

/**
 * Represents a session for a PCV module.
 *
 * Use the {@link openSession | `openSession()`} function to create a session.
 */
export class ClkrstSession {
	#srv: Switch.Service;

	/**
	 * @private
	 */
	constructor(srv: Switch.Service) {
		this.#srv = srv;
	}

	/**
	 * Retrieves the current clock rate of the PCV module.
	 *
	 * @returns The current clock rate of the PCV module in Hz.
	 */
	getClockRate() {
		//Result clkrstGetClockRate(ClkrstSession* session, u32 *out_hz) {
		//    return serviceDispatchOut(&session->s, 8, *out_hz);
		//}
		const outHz = new Uint32Array(1);
		this.#srv.dispatchOut(8, outHz.buffer);
		return outHz[0];
	}

	/**
	 * Returns a list of possible clock rates for the PCV module.
	 *
	 * @param maxCount Determines the size of the `rates` array. Defaults to `20`.
	 * @returns An object containing the list of possible clock rates and their type.
	 */
	getPossibleClockRates(maxCount = 20): {
		type: PcvClockRatesListType;
		rates: Uint32Array;
	} {
		//Result clkrstGetPossibleClockRates(ClkrstSession *session, u32 *rates, s32 max_count, PcvClockRatesListType *out_type, s32 *out_count) {
		//    struct {
		//        s32 type;
		//        s32 count;
		//    } out;
		//
		//    Result rc = serviceDispatchInOut(&session->s, 10, max_count, out,
		//        .buffer_attrs = { SfBufferAttr_Out | SfBufferAttr_HipcAutoSelect, },
		//        .buffers = { { rates, max_count * sizeof(u32) }, }
		//    );
		//}
		const rates = new Uint32Array(maxCount);
		const inArr = new Int32Array([rates.length]);
		const out = new Int32Array(2);
		this.#srv.dispatchInOut(10, inArr.buffer, out.buffer, {
			bufferAttrs: [SfBufferAttr.Out | SfBufferAttr.HipcAutoSelect],
			buffers: [rates.buffer],
		});
		return {
			type: out[0],
			rates: rates.slice(0, out[1]),
		};
	}

	/**
	 * Sets the clock rate of the PCV module to the specified Hz value.
	 *
	 * @param hz The new clock rate to set.
	 */
	setClockRate(hz: number) {
		//Result clkrstSetClockRate(ClkrstSession* session, u32 hz) {
		//    return serviceDispatchIn(&session->s, 7, hz);
		//}
		this.#srv.dispatchIn(7, new Uint32Array([hz]).buffer);
	}
}
