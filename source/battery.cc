#include "error.h"
#include "types.h"

using namespace v8;

namespace {

void nx_battery_init(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Result rc = psmInitialize();
	if (R_FAILED(rc)) {
		nx_throw(iso, "Failed to initialize PSM");
	}
}

void nx_battery_exit(const FunctionCallbackInfo<Value> &info) {
	psmExit();
}

void nx_battery_charging(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	PsmChargerType type;
	Result rc = psmGetChargerType(&type);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "psmGetChargerType");
		return;
	}
	info.GetReturnValue().Set(
	    Boolean::New(iso, type != PsmChargerType_Unconnected));
}

void nx_battery_level(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	double raw_charge;
	Result rc = psmGetRawBatteryChargePercentage(&raw_charge);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "psmGetRawBatteryChargePercentage");
		return;
	}
	info.GetReturnValue().Set(Number::New(iso, raw_charge / 100.0));
}

// Decorate the `BatteryManager` prototype (info[0] is the class).
void nx_battery_init_class(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	Local<Object> proto = info[0]
	                          .As<Object>()
	                          ->Get(context, nx_str(iso, "prototype"))
	                          .ToLocalChecked()
	                          .As<Object>();
	NX_DEF_GET(proto, "charging", nx_battery_charging);
	NX_DEF_GET(proto, "level", nx_battery_level);
}

} // namespace

void nx_init_battery(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "batteryInit", nx_battery_init);
	NX_SET_FUNC(init_obj, "batteryInitClass", nx_battery_init_class);
	NX_SET_FUNC(init_obj, "batteryExit", nx_battery_exit);
}
