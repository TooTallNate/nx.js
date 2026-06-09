#include "hidsys.h"
#include "types.h"
#include <stdio.h>
#include <string.h>

using namespace v8;

namespace {

// Map the HidDeviceTypeBits bitfield (from hidGetNpadDeviceType) to a
// human-readable controller name, mirroring the desktop-browser convention of
// a descriptive `Gamepad.id`. Only the lowest set meaningful bit is used; a
// dual Joy-Con reports both HandheldLeft/Right or JoyLeft/Right, in which case
// we report the pair generically.
const char *device_type_name(u32 bits) {
	if (bits & HidDeviceTypeBits_FullKey)
		return "Nintendo Switch Pro Controller";
	if ((bits & HidDeviceTypeBits_JoyLeft) && (bits & HidDeviceTypeBits_JoyRight))
		return "Joy-Con (L/R)";
	if (bits & HidDeviceTypeBits_JoyLeft)
		return "Joy-Con (L)";
	if (bits & HidDeviceTypeBits_JoyRight)
		return "Joy-Con (R)";
	if ((bits & HidDeviceTypeBits_HandheldLeft) ||
	    (bits & HidDeviceTypeBits_HandheldRight))
		return "Nintendo Switch Handheld";
	if (bits & HidDeviceTypeBits_Palma)
		return "Poke Ball Plus";
	if ((bits & HidDeviceTypeBits_LarkHvcLeft) ||
	    (bits & HidDeviceTypeBits_LarkHvcRight) ||
	    (bits & HidDeviceTypeBits_HandheldLarkHvcLeft) ||
	    (bits & HidDeviceTypeBits_HandheldLarkHvcRight))
		return "Famicom Controller";
	if ((bits & HidDeviceTypeBits_LarkNesLeft) ||
	    (bits & HidDeviceTypeBits_LarkNesRight) ||
	    (bits & HidDeviceTypeBits_HandheldLarkNesLeft) ||
	    (bits & HidDeviceTypeBits_HandheldLarkNesRight))
		return "NES Controller";
	if (bits & HidDeviceTypeBits_Lucia)
		return "SNES Controller";
	if (bits & HidDeviceTypeBits_Lagon)
		return "Nintendo 64 Controller";
	if (bits & HidDeviceTypeBits_Lager)
		return "Sega Genesis Controller";
	return "Nintendo Switch Controller";
}

void nx_gamepad_connection_changed(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_context_t *ctx = nx_ctx(iso);
	bool changed = false;
	if (ctx->hidsys_available) {
		// Non-blocking: returns immediately whether or not the event fired.
		if (R_SUCCEEDED(eventWait(&ctx->hidsys_conn_event, 0))) {
			eventClear(&ctx->hidsys_conn_event);
			// A controller connected or disconnected. Only invalidate the
			// cache for slots that are CURRENTLY connected — those may now host
			// a different controller and must re-resolve. Disconnected slots
			// deliberately KEEP their last-known id so the `gamepaddisconnected`
			// event (dispatched this same sweep) reports the real controller
			// that left, rather than the "switch-gamepad-N" fallback you'd get
			// from querying a pad that is no longer present.
			for (int i = 0; i < 8; i++) {
				if (padIsConnected(&ctx->pads[i]))
					ctx->gamepad_id_cached[i] = false;
			}
			changed = true;
		}
	}
	info.GetReturnValue().Set(Boolean::New(iso, changed));
}

} // namespace

// Read one unique pad's serial number into `out` (NUL-terminated, trimmed).
// Returns true only when a non-empty serial was obtained.
bool read_serial(HidsysUniquePadId pad, char *out, size_t out_len) {
	HidsysUniquePadSerialNumber serial;
	memset(&serial, 0, sizeof(serial));
	if (R_FAILED(hidsysGetUniquePadSerialNumber(pad, &serial)))
		return false;
	// serial_number is a 0x10 byte field, not guaranteed NUL-terminated.
	char ser[sizeof(serial.serial_number) + 1];
	memcpy(ser, serial.serial_number, sizeof(serial.serial_number));
	ser[sizeof(serial.serial_number)] = '\0';
	if (ser[0] == '\0')
		return false;
	snprintf(out, out_len, "%s", ser);
	return true;
}

// Aggregate the serials of EVERY unique pad mapped to Npad `npad` into `out`,
// joined with '+'. A single controller yields one serial; a dual Joy-Con pair
// (handheld or detached dual mode) yields both (e.g. "XAW... + XCW..."), so the
// combined console-as-one-controller has a single stable identifier. Returns
// the number of serials written (0 if none / on failure).
int aggregate_serials(HidNpadIdType npad, char *out, size_t out_len) {
	HidsysUniquePadId pads[2];
	s32 total = 0;
	if (R_FAILED(hidsysGetUniquePadsFromNpad(npad, pads, countof(pads), &total)))
		return 0;
	int written = 0;
	size_t off = 0;
	out[0] = '\0';
	for (s32 i = 0; i < total && i < (s32)countof(pads); i++) {
		char ser[sizeof(HidsysUniquePadSerialNumber) + 1];
		if (!read_serial(pads[i], ser, sizeof(ser)))
			continue;
		int n = snprintf(out + off, out_len - off, "%s%s",
		                 written ? " + " : "", ser);
		if (n < 0 || (size_t)n >= out_len - off)
			break;
		off += n;
		written++;
	}
	return written;
}

void nx_gamepad_resolve_id(Isolate *iso, unsigned index, char *out,
                           size_t out_len) {
	nx_context_t *ctx = nx_ctx(iso);

	if (index < 8 && ctx->gamepad_id_cached[index]) {
		snprintf(out, out_len, "%s", ctx->gamepad_id_cache[index]);
		return;
	}

	// Fallback used whenever the real serial can't be obtained (hidsys
	// unavailable, firmware < 5.0.0, no paired unique pad, IPC failure). This
	// preserves a non-empty, unique-per-slot id (the prior behavior).
	char resolved[96];
	snprintf(resolved, sizeof(resolved), "switch-gamepad-%u", index);
	bool real = false;

	if (ctx->hidsys_available) {
		// Slot 0 doubles as the handheld controller. In handheld mode the
		// active controller lives under HidNpadIdType_Handheld (0x20), not
		// No1 (0), so prefer whichever is currently connected for that slot.
		HidNpadIdType npad = (HidNpadIdType)index;
		if (index == 0) {
			u32 style = hidGetNpadStyleSet(HidNpadIdType_Handheld);
			if (style & HidNpadStyleTag_NpadHandheld)
				npad = HidNpadIdType_Handheld;
		}

		char serials[48];
		int n = aggregate_serials(npad, serials, sizeof(serials));
		// Fall back to the regular Npad if the handheld query came up empty
		// (e.g. a controller connected to slot 0 in non-handheld mode).
		if (n == 0 && npad != (HidNpadIdType)index)
			n = aggregate_serials((HidNpadIdType)index, serials,
			                      sizeof(serials));
		if (n > 0) {
			const char *name = device_type_name(hidGetNpadDeviceType(npad));
			snprintf(resolved, sizeof(resolved), "%s (%s)", name, serials);
			real = true;
		}
	}

	// Only cache a successfully-resolved real serial. The serial DB can lag a
	// frame or two behind a freshly-connected controller, so caching the
	// fallback here would "stick" it until the next connect/disconnect event.
	// Leaving the slot uncached lets the next `id` read retry — cheap, since
	// retries only happen for an as-yet-unresolved connected slot.
	if (real && index < 8) {
		snprintf(ctx->gamepad_id_cache[index],
		         sizeof(ctx->gamepad_id_cache[index]), "%s", resolved);
		ctx->gamepad_id_cached[index] = true;
	}
	snprintf(out, out_len, "%s", resolved);
}

void nx_hidsys_init(Isolate *iso) {
	nx_context_t *ctx = nx_ctx(iso);
	ctx->hidsys_available = false;
	memset(ctx->gamepad_id_cached, 0, sizeof(ctx->gamepad_id_cached));

	Result rc = hidsysInitialize();
	if (R_FAILED(rc))
		return;

	rc = hidsysAcquireUniquePadConnectionEventHandle(&ctx->hidsys_conn_event);
	if (R_FAILED(rc)) {
		hidsysExit();
		return;
	}

	ctx->hidsys_available = true;
}

void nx_hidsys_exit(nx_context_t *ctx) {
	if (!ctx->hidsys_available)
		return;
	eventClose(&ctx->hidsys_conn_event);
	hidsysExit();
	ctx->hidsys_available = false;
}

void nx_init_hidsys(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "gamepadConnectionChanged",
	            nx_gamepad_connection_changed);
}
