#include "error.h"
#include "types.h"

using namespace v8;

namespace {

void nx_applet_illuminance(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	float illuminance = 0.0f;
	Result rc = appletGetCurrentIlluminance(&illuminance);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "appletGetCurrentIlluminance");
		return;
	}
	info.GetReturnValue().Set(Number::New(iso, illuminance));
}

void nx_applet_get_applet_type(const FunctionCallbackInfo<Value> &info) {
	info.GetReturnValue().Set(
	    Integer::New(info.GetIsolate(), appletGetAppletType()));
}

void nx_applet_get_operation_mode(const FunctionCallbackInfo<Value> &info) {
	info.GetReturnValue().Set(
	    Integer::New(info.GetIsolate(), appletGetOperationMode()));
}

void nx_applet_set_media_playback_state(
    const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	bool state = info[0]->BooleanValue(iso);
	Result rc = appletSetMediaPlaybackState(state);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "appletSetMediaPlaybackState");
	}
}

} // namespace

void nx_init_applet(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "appletIlluminance", nx_applet_illuminance);
	NX_SET_FUNC(init_obj, "appletGetAppletType", nx_applet_get_applet_type);
	NX_SET_FUNC(init_obj, "appletGetOperationMode",
	            nx_applet_get_operation_mode);
	NX_SET_FUNC(init_obj, "appletSetMediaPlaybackState",
	            nx_applet_set_media_playback_state);
}
