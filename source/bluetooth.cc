// Web Bluetooth API native bindings — BLE GATT client over the Switch's
// application-facing Bluetooth services:
//
//   - `btm.u` (btmu): BLE scanning, connection management, pairing, and GATT
//     metadata discovery (services / characteristics / descriptors).
//   - `bt`: GATT client data operations (read / write / notifications) and
//     the LE event stream that carries operation completions + notification
//     values (btRegisterBleEvent / btGetLeEventInfo).
//
// These are the same services first-party titles use for BLE accessories
// (e.g. the Poké Ball Plus), available to applications on [5.0.0+].
//
// Architecture: the native layer is a thin, mostly-stateless wrapper; the
// Web Bluetooth state machine (device selection, promise correlation,
// notification routing) lives in TypeScript. JS pumps `$.btlePollEvents()`
// on an interval while anything is in flight; the poll drains the btmu
// status events and the bt LE event stream into plain JS event objects.
//
// This module is Switch-only (stubbed in the host nxjs-test binary).
#include "bluetooth.h"
#include "error.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

using namespace v8;

namespace {

struct {
	bool initialized;
	Event scan_event;       // btmuAcquireBleScanEvent (autoclear)
	Event connection_event; // btmuAcquireBleConnectionEvent (autoclear)
	Event discovery_event;  // btmuAcquireBleServiceDiscoveryEvent (autoclear)
	Event mtu_event;        // btmuAcquireBleMtuConfigEvent (autoclear)
	Event ble_event;        // btRegisterBleEvent (autoclear)
	// Active scan mode: 0 = none, 1 = "general" (Nintendo accessory
	// filter), 2 = "smart device" (filtered by advertised service UUID).
	int scan_mode;
	BtdrvGattAttributeUuid scan_uuid; // smart-device scan filter
} g;

// ---------------------------------------------------------------------------
// UUID / address conversion
// ---------------------------------------------------------------------------

// The Switch Bluetooth stack is bluedroid-derived: 128-bit UUIDs are stored
// little-endian (reversed relative to the canonical string form).
void uuid_to_string(const BtdrvGattAttributeUuid *uuid, char *out /*[37]*/) {
	u8 full[16];
	if (uuid->size == 0x10) {
		memcpy(full, uuid->uuid, 16);
	} else {
		// Expand a 16/32-bit alias onto the Bluetooth Base UUID
		// (0000xxxx-0000-1000-8000-00805f9b34fb), little-endian.
		static const u8 base[16] = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00,
		                            0x00, 0x80, 0x00, 0x10, 0x00, 0x00,
		                            0x00, 0x00, 0x00, 0x00};
		memcpy(full, base, 16);
		u32 alias = 0;
		if (uuid->size == 0x2)
			alias = uuid->uuid[0] | (uuid->uuid[1] << 8);
		else if (uuid->size == 0x4)
			alias = uuid->uuid[0] | (uuid->uuid[1] << 8) |
			        (uuid->uuid[2] << 16) | ((u32)uuid->uuid[3] << 24);
		full[12] = (u8)(alias & 0xff);
		full[13] = (u8)((alias >> 8) & 0xff);
		full[14] = (u8)((alias >> 16) & 0xff);
		full[15] = (u8)((alias >> 24) & 0xff);
	}
	// Reverse to big-endian canonical ordering.
	u8 be[16];
	for (int i = 0; i < 16; i++)
		be[i] = full[15 - i];
	snprintf(out, 37,
	         "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-"
	         "%02x%02x%02x%02x%02x%02x",
	         be[0], be[1], be[2], be[3], be[4], be[5], be[6], be[7], be[8],
	         be[9], be[10], be[11], be[12], be[13], be[14], be[15]);
}

int hex_nibble(char c) {
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

// Parse a canonical UUID string into the (little-endian) native form.
bool uuid_from_string(const char *str, BtdrvGattAttributeUuid *out) {
	u8 be[16];
	int nibbles = 0;
	for (const char *p = str; *p; p++) {
		if (*p == '-')
			continue;
		int v = hex_nibble(*p);
		if (v < 0 || nibbles >= 32)
			return false;
		if (nibbles % 2 == 0)
			be[nibbles / 2] = (u8)(v << 4);
		else
			be[nibbles / 2] |= (u8)v;
		nibbles++;
	}
	if (nibbles != 32)
		return false;
	out->size = 0x10;
	for (int i = 0; i < 16; i++)
		out->uuid[i] = be[15 - i];
	return true;
}

void addr_to_string(const BtdrvAddress *addr, char *out /*[18]*/) {
	snprintf(out, 18, "%02x:%02x:%02x:%02x:%02x:%02x", addr->address[0],
	         addr->address[1], addr->address[2], addr->address[3],
	         addr->address[4], addr->address[5]);
}

bool addr_from_string(const char *str, BtdrvAddress *out) {
	int values[6];
	if (sscanf(str, "%2x:%2x:%2x:%2x:%2x:%2x", &values[0], &values[1],
	           &values[2], &values[3], &values[4], &values[5]) != 6)
		return false;
	for (int i = 0; i < 6; i++)
		out->address[i] = (u8)values[i];
	return true;
}

// Build a BtdrvGattId from JS-provided uuid string + instance id.
bool gatt_id_from_args(Isolate *iso, Local<Value> uuid_val,
                       Local<Value> instance_val, BtdrvGattId *out) {
	Local<Context> context = iso->GetCurrentContext();
	String::Utf8Value uuid_str(iso, uuid_val);
	memset(out, 0, sizeof(*out));
	if (!*uuid_str || !uuid_from_string(*uuid_str, &out->uuid)) {
		nx_throw(iso, "invalid UUID");
		return false;
	}
	int32_t instance = 0;
	if (!instance_val->Int32Value(context).To(&instance))
		return false;
	out->instance_id = (u8)instance;
	return true;
}

// ---------------------------------------------------------------------------
// Init / teardown
// ---------------------------------------------------------------------------

void btle_teardown(void) {
	if (!g.initialized)
		return;
	eventClose(&g.ble_event);
	eventClose(&g.mtu_event);
	eventClose(&g.discovery_event);
	eventClose(&g.connection_event);
	eventClose(&g.scan_event);
	btExit();
	btmuExit();
	g.initialized = false;
}

void nx_btle_init(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	if (g.initialized)
		return;
	if (hosversionBefore(5, 0, 0)) {
		nx_throw(iso, "Bluetooth LE requires system firmware 5.0.0+");
		return;
	}
	Result rc = btmuInitialize();
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "btmuInitialize");
		return;
	}
	rc = btInitialize();
	if (R_FAILED(rc)) {
		btmuExit();
		nx_throw_libnx_error(iso, rc, "btInitialize");
		return;
	}
	rc = btmuAcquireBleScanEvent(&g.scan_event);
	if (R_SUCCEEDED(rc))
		rc = btmuAcquireBleConnectionEvent(&g.connection_event);
	if (R_SUCCEEDED(rc))
		rc = btmuAcquireBleServiceDiscoveryEvent(&g.discovery_event);
	if (R_SUCCEEDED(rc))
		rc = btmuAcquireBleMtuConfigEvent(&g.mtu_event);
	if (R_SUCCEEDED(rc))
		rc = btRegisterBleEvent(&g.ble_event);
	if (R_FAILED(rc)) {
		btle_teardown();
		g.initialized = false;
		nx_throw_libnx_error(iso, rc, "btle event acquisition");
		return;
	}
	g.initialized = true;
}

void nx_btle_exit(const FunctionCallbackInfo<Value> &info) {
	btle_teardown();
}

#define BTLE_GUARD()                                                           \
	do {                                                                       \
		if (!g.initialized) {                                                  \
			nx_throw(iso, "Bluetooth not initialized");                        \
			return;                                                            \
		}                                                                      \
	} while (0)

// ---------------------------------------------------------------------------
// Scanning
// ---------------------------------------------------------------------------

// btleScanStart(serviceUuid?: string, companyId?: number, pattern?: ArrayBuffer)
//
// Three modes (the Switch's application BLE scanner is filter-based; there
// is no unfiltered scan for applications):
//   - serviceUuid string: "smart device" scan — matches devices advertising
//     that 128-bit service UUID.
//   - companyId (+ up to 6 `pattern` bytes): "general" scan with a custom
//     manufacturer-data filter (maps Web Bluetooth `manufacturerData`
//     filters).
//   - neither: "general" scan with Nintendo's stock accessory parameter.
void nx_btle_scan_start(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	BTLE_GUARD();
	if (g.scan_mode != 0) {
		nx_throw(iso, "a BLE scan is already active");
		return;
	}
	Result rc;
	if (info[0]->IsString()) {
		String::Utf8Value uuid_str(iso, info[0]);
		if (!*uuid_str || !uuid_from_string(*uuid_str, &g.scan_uuid)) {
			nx_throw(iso, "invalid UUID");
			return;
		}
		rc = btmuStartBleScanForSmartDevice(&g.scan_uuid);
		if (R_FAILED(rc)) {
			nx_throw_libnx_error(iso, rc, "btmuStartBleScanForSmartDevice");
			return;
		}
		g.scan_mode = 2;
	} else {
		BtdrvBleAdvertisePacketParameter param;
		if (info[1]->IsNumber()) {
			memset(&param, 0, sizeof(param));
			uint32_t company = 0;
			if (!info[1]->Uint32Value(context).To(&company))
				return;
			param.company_id = (u16)company;
			if (!info[2]->IsUndefined() && !info[2]->IsNull()) {
				size_t size = 0;
				uint8_t *buf = NX_GetBufferSource(iso, &size, info[2]);
				if (!buf) {
					nx_throw(iso, "expected ArrayBuffer pattern");
					return;
				}
				if (size > sizeof(param.pattern_data))
					size = sizeof(param.pattern_data);
				memcpy(param.pattern_data, buf, size);
			}
		} else {
			rc = btmuGetBleScanFilterParameter(0x1, &param);
			if (R_FAILED(rc)) {
				nx_throw_libnx_error(iso, rc,
				                     "btmuGetBleScanFilterParameter");
				return;
			}
		}
		rc = btmuStartBleScanForGeneral(param);
		if (R_FAILED(rc)) {
			nx_throw_libnx_error(iso, rc, "btmuStartBleScanForGeneral");
			return;
		}
		g.scan_mode = 1;
	}
}

void nx_btle_scan_stop(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	BTLE_GUARD();
	Result rc = 0;
	if (g.scan_mode == 2) {
		rc = btmuStopBleScanForSmartDevice();
	} else if (g.scan_mode == 1) {
		rc = btmuStopBleScanForGeneral();
	}
	g.scan_mode = 0;
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "btmuStopBleScan");
		return;
	}
}

// Extract the local name from the raw scan-result blob: scan for plausible
// BLE AD structures (length / type 0x08 "shortened" or 0x09 "complete" local
// name / printable payload).
bool scan_result_name(const BtdrvBleScanResult *result, char *out,
                      size_t out_size) {
	const u8 *raw = (const u8 *)result;
	const size_t raw_size = sizeof(*result);
	for (size_t i = 0; i + 2 < raw_size; i++) {
		u8 len = raw[i];
		u8 type = raw[i + 1];
		if ((type != 0x08 && type != 0x09) || len < 2 || len > 0x1e)
			continue;
		size_t name_len = (size_t)len - 1;
		if (i + 2 + name_len > raw_size)
			continue;
		bool printable = true;
		for (size_t j = 0; j < name_len; j++) {
			u8 c = raw[i + 2 + j];
			if (c < 0x20 || c > 0x7e) {
				printable = false;
				break;
			}
		}
		if (!printable)
			continue;
		if (name_len >= out_size)
			name_len = out_size - 1;
		memcpy(out, raw + i + 2, name_len);
		out[name_len] = '\0';
		return true;
	}
	return false;
}

void nx_btle_scan_results(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	BTLE_GUARD();
	BtdrvBleScanResult results[16];
	u8 total = 0;
	Result rc;
	if (g.scan_mode == 2) {
		rc = btmuGetBleScanResultsForSmartDevice(results, 16, &total);
	} else {
		rc = btmuGetBleScanResultsForGeneral(results, 16, &total);
	}
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "btmuGetBleScanResults");
		return;
	}
	if (total > 16)
		total = 16;
	bool include_raw = info[0]->BooleanValue(iso);
	Local<Array> arr = Array::New(iso, total);
	for (u8 i = 0; i < total; i++) {
		Local<Object> obj = Object::New(iso);
		char addr[18];
		addr_to_string(&results[i].addr, addr);
		obj->Set(context, nx_str(iso, "address"), nx_str_lossy(iso, addr))
		    .Check();
		char name[0x40];
		if (scan_result_name(&results[i], name, sizeof(name))) {
			obj->Set(context, nx_str(iso, "name"), nx_str_lossy(iso, name))
			    .Check();
		}
		if (include_raw) {
			// Diagnostic: the full raw BtdrvBleScanResult bytes.
			void *copy = nx_alloc(iso, sizeof(BtdrvBleScanResult));
			if (!copy)
				return;
			memcpy(copy, &results[i], sizeof(BtdrvBleScanResult));
			std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
			    copy, sizeof(BtdrvBleScanResult),
			    [](void *p, size_t, void *) { free(p); }, nullptr);
			obj->Set(context, nx_str(iso, "raw"),
			         ArrayBuffer::New(iso, std::move(bs)))
			    .Check();
		}
		arr->Set(context, i, obj).Check();
	}
	info.GetReturnValue().Set(arr);
}

// ---------------------------------------------------------------------------
// Connections
// ---------------------------------------------------------------------------

void nx_btle_connect(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	BTLE_GUARD();
	String::Utf8Value addr_str(iso, info[0]);
	BtdrvAddress addr;
	if (!*addr_str || !addr_from_string(*addr_str, &addr)) {
		nx_throw(iso, "invalid Bluetooth address");
		return;
	}
	Result rc = btmuBleConnect(addr);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "btmuBleConnect");
		return;
	}
}

void nx_btle_disconnect(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	BTLE_GUARD();
	uint32_t handle = 0;
	if (!info[0]->Uint32Value(context).To(&handle))
		return;
	Result rc = btmuBleDisconnect(handle);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "btmuBleDisconnect");
		return;
	}
}

void nx_btle_connections(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	BTLE_GUARD();
	BtdrvBleConnectionInfo infos[16] = {};
	u8 total = 0;
	Result rc = btmuBleGetConnectionState(infos, 16, &total);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "btmuBleGetConnectionState");
		return;
	}
	if (total > 16)
		total = 16;
	Local<Array> arr = Array::New(iso);
	uint32_t count = 0;
	for (u8 i = 0; i < total; i++) {
		if (infos[i].connection_handle == 0xFFFFFFFF)
			continue;
		Local<Object> obj = Object::New(iso);
		obj->Set(context, nx_str(iso, "handle"),
		         Integer::NewFromUnsigned(iso, infos[i].connection_handle))
		    .Check();
		char addr[18];
		addr_to_string(&infos[i].addr, addr);
		obj->Set(context, nx_str(iso, "address"), nx_str_lossy(iso, addr))
		    .Check();
		arr->Set(context, count++, obj).Check();
	}
	info.GetReturnValue().Set(arr);
}

// ---------------------------------------------------------------------------
// GATT discovery
// ---------------------------------------------------------------------------

void nx_btle_get_services(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	BTLE_GUARD();
	uint32_t conn = 0;
	if (!info[0]->Uint32Value(context).To(&conn))
		return;
	BtmGattService services[16] = {};
	u8 total = 0;
	Result rc = btmuGetGattServices(conn, services, 16, &total);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "btmuGetGattServices");
		return;
	}
	if (total > 16)
		total = 16;
	Local<Array> arr = Array::New(iso, total);
	for (u8 i = 0; i < total; i++) {
		Local<Object> obj = Object::New(iso);
		char uuid[37];
		uuid_to_string(&services[i].uuid, uuid);
		obj->Set(context, nx_str(iso, "uuid"), nx_str_lossy(iso, uuid))
		    .Check();
		obj->Set(context, nx_str(iso, "handle"),
		         Integer::NewFromUnsigned(iso, services[i].handle))
		    .Check();
		obj->Set(context, nx_str(iso, "instanceId"),
		         Integer::NewFromUnsigned(iso, services[i].instance_id))
		    .Check();
		obj->Set(context, nx_str(iso, "primary"),
		         Boolean::New(iso, services[i].primary_service != 0))
		    .Check();
		arr->Set(context, i, obj).Check();
	}
	info.GetReturnValue().Set(arr);
}

void nx_btle_get_characteristics(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	BTLE_GUARD();
	uint32_t conn = 0, svc_handle = 0;
	if (!info[0]->Uint32Value(context).To(&conn) ||
	    !info[1]->Uint32Value(context).To(&svc_handle))
		return;
	BtmGattCharacteristic chars[32] = {};
	u8 total = 0;
	Result rc =
	    btmuGetGattCharacteristics(conn, (u16)svc_handle, chars, 32, &total);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "btmuGetGattCharacteristics");
		return;
	}
	if (total > 32)
		total = 32;
	Local<Array> arr = Array::New(iso, total);
	for (u8 i = 0; i < total; i++) {
		Local<Object> obj = Object::New(iso);
		char uuid[37];
		uuid_to_string(&chars[i].uuid, uuid);
		obj->Set(context, nx_str(iso, "uuid"), nx_str_lossy(iso, uuid))
		    .Check();
		obj->Set(context, nx_str(iso, "handle"),
		         Integer::NewFromUnsigned(iso, chars[i].handle))
		    .Check();
		obj->Set(context, nx_str(iso, "instanceId"),
		         Integer::NewFromUnsigned(iso, chars[i].instance_id))
		    .Check();
		// NOTE: libnx's BtmGattCharacteristic places `properties` at offset
		// 0x1E, but on-device the GATT properties byte actually lives at
		// offset 0x20 (verified against a device whose characteristic is
		// read+writeWithoutResponse+notify = 0x16). Read it positionally
		// until the libnx struct is corrected upstream.
		obj->Set(context, nx_str(iso, "properties"),
		         Integer::NewFromUnsigned(
		             iso, ((const u8 *)&chars[i])[0x20]))
		    .Check();
		arr->Set(context, i, obj).Check();
	}
	info.GetReturnValue().Set(arr);
}

void nx_btle_get_descriptors(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	BTLE_GUARD();
	uint32_t conn = 0, char_handle = 0;
	if (!info[0]->Uint32Value(context).To(&conn) ||
	    !info[1]->Uint32Value(context).To(&char_handle))
		return;
	BtmGattDescriptor descs[16] = {};
	u8 total = 0;
	Result rc =
	    btmuGetGattDescriptors(conn, (u16)char_handle, descs, 16, &total);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "btmuGetGattDescriptors");
		return;
	}
	if (total > 16)
		total = 16;
	Local<Array> arr = Array::New(iso, total);
	for (u8 i = 0; i < total; i++) {
		Local<Object> obj = Object::New(iso);
		char uuid[37];
		uuid_to_string(&descs[i].uuid, uuid);
		obj->Set(context, nx_str(iso, "uuid"), nx_str_lossy(iso, uuid))
		    .Check();
		obj->Set(context, nx_str(iso, "handle"),
		         Integer::NewFromUnsigned(iso, descs[i].handle))
		    .Check();
		arr->Set(context, i, obj).Check();
	}
	info.GetReturnValue().Set(arr);
}

// ---------------------------------------------------------------------------
// GATT client data operations (completions arrive via the LE event stream)
// ---------------------------------------------------------------------------

// (conn, primary, svcUuid, svcInstance, charUuid, charInstance, ...)
bool gatt_op_args(const FunctionCallbackInfo<Value> &info, uint32_t *conn,
                  bool *primary, BtdrvGattId *serv_id, BtdrvGattId *char_id) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	if (!info[0]->Uint32Value(context).To(conn))
		return false;
	*primary = info[1]->BooleanValue(iso);
	if (!gatt_id_from_args(iso, info[2], info[3], serv_id))
		return false;
	if (!gatt_id_from_args(iso, info[4], info[5], char_id))
		return false;
	return true;
}

void nx_btle_read(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	BTLE_GUARD();
	uint32_t conn;
	bool primary;
	BtdrvGattId serv_id, char_id;
	if (!gatt_op_args(info, &conn, &primary, &serv_id, &char_id))
		return;
	Result rc = btLeClientReadCharacteristic(conn, primary, &serv_id,
	                                         &char_id, 0 /* auth_req */);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "btLeClientReadCharacteristic");
		return;
	}
}

void nx_btle_write(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	BTLE_GUARD();
	uint32_t conn;
	bool primary;
	BtdrvGattId serv_id, char_id;
	if (!gatt_op_args(info, &conn, &primary, &serv_id, &char_id))
		return;
	size_t size = 0;
	uint8_t *buf = NX_GetBufferSource(iso, &size, info[6]);
	if (!buf) {
		nx_throw(iso, "expected ArrayBuffer");
		return;
	}
	if (size > 0x258) {
		iso->ThrowException(Exception::RangeError(
		    nx_str(iso, "write value exceeds 600 bytes")));
		return;
	}
	bool with_response = info[7]->BooleanValue(iso);
	Result rc = btLeClientWriteCharacteristic(conn, primary, &serv_id,
	                                          &char_id, buf, size,
	                                          0 /* auth_req */, with_response);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "btLeClientWriteCharacteristic");
		return;
	}
}

// (conn, primary, svcUuid, svcInst, charUuid, charInst, descUuid, descInst,
//  data)
void nx_btle_write_descriptor(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	BTLE_GUARD();
	uint32_t conn;
	bool primary;
	BtdrvGattId serv_id, char_id, desc_id;
	if (!gatt_op_args(info, &conn, &primary, &serv_id, &char_id))
		return;
	if (!gatt_id_from_args(iso, info[6], info[7], &desc_id))
		return;
	size_t size = 0;
	uint8_t *buf = NX_GetBufferSource(iso, &size, info[8]);
	if (!buf) {
		nx_throw(iso, "expected ArrayBuffer");
		return;
	}
	if (size > 0x258) {
		iso->ThrowException(Exception::RangeError(
		    nx_str(iso, "write value exceeds 600 bytes")));
		return;
	}
	Result rc = btLeClientWriteDescriptor(conn, primary, &serv_id, &char_id,
	                                      &desc_id, buf, size,
	                                      0 /* auth_req */);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "btLeClientWriteDescriptor");
		return;
	}
}

void nx_btle_notify(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	BTLE_GUARD();
	uint32_t conn;
	bool primary;
	BtdrvGattId serv_id, char_id;
	if (!gatt_op_args(info, &conn, &primary, &serv_id, &char_id))
		return;
	bool enable = info[6]->BooleanValue(iso);
	Result rc;
	if (enable) {
		rc = btLeClientRegisterNotification(conn, primary, &serv_id,
		                                    &char_id);
	} else {
		rc = btLeClientDeregisterNotification(conn, primary, &serv_id,
		                                      &char_id);
	}
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc,
		                     enable ? "btLeClientRegisterNotification"
		                            : "btLeClientDeregisterNotification");
		return;
	}
}

// ---------------------------------------------------------------------------
// Event pump
// ---------------------------------------------------------------------------

void push_event(Isolate *iso, Local<Context> context, Local<Array> arr,
                uint32_t *count, Local<Object> obj) {
	arr->Set(context, (*count)++, obj).Check();
}

Local<Object> simple_event(Isolate *iso, Local<Context> context,
                           const char *type) {
	Local<Object> obj = Object::New(iso);
	obj->Set(context, nx_str(iso, "type"), nx_str(iso, type)).Check();
	return obj;
}

// btleConfigureMtu(conn, mtu)
//
// Requests an ATT MTU for the connection (completion via the MTU event).
// Without this the default MTU of 23 caps writes at 20 bytes — browsers
// negotiate a large MTU automatically, so Web Bluetooth apps assume it.
void nx_btle_configure_mtu(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	BTLE_GUARD();
	uint32_t conn = 0, mtu = 0;
	if (!info[0]->Uint32Value(context).To(&conn) ||
	    !info[1]->Uint32Value(context).To(&mtu))
		return;
	Result rc = btmuConfigureBleMtu(conn, (u16)mtu);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "btmuConfigureBleMtu");
		return;
	}
}

void nx_btle_get_mtu(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	BTLE_GUARD();
	uint32_t conn = 0;
	if (!info[0]->Uint32Value(context).To(&conn))
		return;
	u16 mtu = 0;
	Result rc = btmuGetBleMtu(conn, &mtu);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "btmuGetBleMtu");
		return;
	}
	info.GetReturnValue().Set(Integer::NewFromUnsigned(iso, mtu));
}

// btleRegisterDataPath(uuid, register)
//
// Registers a BLE GATT data path for a service UUID with btm. This is how an
// application associates itself with a GATT service: btm maps registered
// data paths to btdrv GATT client interfaces (RegisterGattClient), which
// connections are keyed on — without one, BleConnect is accepted but never
// completes.
void nx_btle_register_data_path(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	BTLE_GUARD();
	BtmBleDataPath path = {};
	String::Utf8Value uuid_str(iso, info[0]);
	if (!*uuid_str || !uuid_from_string(*uuid_str, &path.uuid)) {
		nx_throw(iso, "invalid UUID");
		return;
	}
	bool reg = info[1]->BooleanValue(iso);
	Result rc = reg ? btmuRegisterBleGattDataPath(&path)
	                : btmuUnregisterBleGattDataPath(&path);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc,
		                     reg ? "btmuRegisterBleGattDataPath"
		                         : "btmuUnregisterBleGattDataPath");
		return;
	}
}

// Raw btdrv-level BLE scan. The application-facing btm.u scanner is
// filter-based and cannot discover devices that advertise neither Nintendo
// manufacturer data nor a 128-bit service UUID — but `btmuBleConnect` needs
// the peer in the Bluetooth stack's device cache (for its address type).
// Running an unfiltered scan at the btdrv level (no BLE event registration —
// btm-sysmodule keeps that) populates the shared bluedroid cache, after
// which btm.u connects by address succeed. hbloader grants homebrew access
// to btdrv.
void nx_btle_raw_scan_start(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Result rc = btdrvInitialize();
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "btdrvInitialize");
		return;
	}
	// Drop btm's leftover scan-filter conditions and disable filtering
	// entirely so every advertisement is processed into the device cache
	// (including the peer's address type, which BleConnect needs — devices
	// with random addresses cannot be connected without it). btm re-adds
	// its conditions on its next scan start.
	btdrvClearBleScanFilters();
	btdrvEnableBleScanFilter(false);
	rc = btdrvStartBleScan();
	if (R_FAILED(rc)) {
		btdrvExit();
		nx_throw_libnx_error(iso, rc, "btdrvStartBleScan");
		return;
	}
}

void nx_btle_raw_scan_stop(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Result rc = btdrvStopBleScan();
	btdrvEnableBleScanFilter(true);
	btdrvExit();
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "btdrvStopBleScan");
		return;
	}
}

void nx_btle_poll_events(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	if (!g.initialized) {
		info.GetReturnValue().Set(Array::New(iso, 0));
		return;
	}
	Local<Array> arr = Array::New(iso);
	uint32_t count = 0;

	if (R_SUCCEEDED(eventWait(&g.scan_event, 0)))
		push_event(iso, context, arr, &count,
		           simple_event(iso, context, "scan"));
	if (R_SUCCEEDED(eventWait(&g.connection_event, 0)))
		push_event(iso, context, arr, &count,
		           simple_event(iso, context, "connection"));
	if (R_SUCCEEDED(eventWait(&g.discovery_event, 0)))
		push_event(iso, context, arr, &count,
		           simple_event(iso, context, "discovery"));
	if (R_SUCCEEDED(eventWait(&g.mtu_event, 0)))
		push_event(iso, context, arr, &count,
		           simple_event(iso, context, "mtu"));

	// Drain the bt LE event stream (GATT op completions + notifications).
	// Each btGetLeEventInfo call consumes one pending event.
	for (int spin = 0; spin < 16; spin++) {
		if (R_FAILED(eventWait(&g.ble_event, 0)))
			break;
		static u8 event_buf[0x400];
		BtdrvBleEventType type;
		Result rc = btGetLeEventInfo(event_buf, sizeof(event_buf), &type);
		if (R_FAILED(rc))
			break;
		const BtdrvLeEventInfo *le = (const BtdrvLeEventInfo *)event_buf;
		Local<Object> obj = Object::New(iso);
		obj->Set(context, nx_str(iso, "type"), nx_str(iso, "gatt")).Check();
		obj->Set(context, nx_str(iso, "event"),
		         Integer::New(iso, (int)type))
		    .Check();
		obj->Set(context, nx_str(iso, "status"),
		         Integer::NewFromUnsigned(iso, le->unk_x0))
		    .Check();
		obj->Set(context, nx_str(iso, "connId"),
		         Integer::NewFromUnsigned(iso, le->unk_x4))
		    .Check();
		obj->Set(context, nx_str(iso, "op"),
		         Integer::NewFromUnsigned(iso, le->unk_x8))
		    .Check();
		char uuid[37];
		uuid_to_string(&le->uuid0, uuid);
		obj->Set(context, nx_str(iso, "serviceUuid"), nx_str_lossy(iso, uuid))
		    .Check();
		uuid_to_string(&le->uuid1, uuid);
		obj->Set(context, nx_str(iso, "characteristicUuid"),
		         nx_str_lossy(iso, uuid))
		    .Check();
		uuid_to_string(&le->uuid2, uuid);
		obj->Set(context, nx_str(iso, "descriptorUuid"),
		         nx_str_lossy(iso, uuid))
		    .Check();
		u16 size = le->size;
		if (size > sizeof(le->data))
			size = sizeof(le->data);
		void *copy = nx_alloc(iso, size > 0 ? size : 1);
		if (!copy)
			return;
		memcpy(copy, le->data, size);
		std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
		    copy, size, [](void *p, size_t, void *) { free(p); }, nullptr);
		obj->Set(context, nx_str(iso, "data"),
		         ArrayBuffer::New(iso, std::move(bs)))
		    .Check();
		push_event(iso, context, arr, &count, obj);
	}

	info.GetReturnValue().Set(arr);
}

} // namespace

void nx_init_bluetooth(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "btleInit", nx_btle_init);
	NX_SET_FUNC(init_obj, "btleExit", nx_btle_exit);
	NX_SET_FUNC(init_obj, "btleScanStart", nx_btle_scan_start);
	NX_SET_FUNC(init_obj, "btleScanStop", nx_btle_scan_stop);
	NX_SET_FUNC(init_obj, "btleScanResults", nx_btle_scan_results);
	NX_SET_FUNC(init_obj, "btleConnect", nx_btle_connect);
	NX_SET_FUNC(init_obj, "btleDisconnect", nx_btle_disconnect);
	NX_SET_FUNC(init_obj, "btleConnections", nx_btle_connections);
	NX_SET_FUNC(init_obj, "btleGetServices", nx_btle_get_services);
	NX_SET_FUNC(init_obj, "btleGetCharacteristics",
	            nx_btle_get_characteristics);
	NX_SET_FUNC(init_obj, "btleGetDescriptors", nx_btle_get_descriptors);
	NX_SET_FUNC(init_obj, "btleRead", nx_btle_read);
	NX_SET_FUNC(init_obj, "btleWrite", nx_btle_write);
	NX_SET_FUNC(init_obj, "btleWriteDescriptor", nx_btle_write_descriptor);
	NX_SET_FUNC(init_obj, "btleNotify", nx_btle_notify);
	NX_SET_FUNC(init_obj, "btlePollEvents", nx_btle_poll_events);
	NX_SET_FUNC(init_obj, "btleConfigureMtu", nx_btle_configure_mtu);
	NX_SET_FUNC(init_obj, "btleGetMtu", nx_btle_get_mtu);
	NX_SET_FUNC(init_obj, "btleRegisterDataPath", nx_btle_register_data_path);
	NX_SET_FUNC(init_obj, "btleRawScanStart", nx_btle_raw_scan_start);
	NX_SET_FUNC(init_obj, "btleRawScanStop", nx_btle_raw_scan_stop);
}
