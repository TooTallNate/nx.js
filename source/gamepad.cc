#include "error.h"
#include "types.h"
#include "wrap.h"
#include <stdio.h>

using namespace v8;

namespace {

typedef struct {
	HidNpadIdType id;
	PadState *pad;
} nx_gamepad_t;

typedef struct {
	nx_gamepad_t *gamepad;
	u64 mask;
} nx_gamepad_button_t;

// Web Gamepad index order -> Switch HidNpadButton masks (see AGENTS.md note).
const u64 standard_button_masks[] = {
    HidNpadButton_B,     HidNpadButton_A,      HidNpadButton_Y,
    HidNpadButton_X,     HidNpadButton_L,      HidNpadButton_R,
    HidNpadButton_ZL,    HidNpadButton_ZR,     HidNpadButton_Minus,
    HidNpadButton_Plus,  HidNpadButton_StickL, HidNpadButton_StickR,
    HidNpadButton_Up,    HidNpadButton_Down,   HidNpadButton_Left,
    HidNpadButton_Right,
};

void nx_gamepad_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	uint32_t id = 0;
	if (!info[0]->Uint32Value(context).To(&id))
		return;
	nx_gamepad_t *gamepad =
	    static_cast<nx_gamepad_t *>(calloc(1, sizeof(nx_gamepad_t)));
	gamepad->id = (HidNpadIdType)id;
	gamepad->pad = &nx_ctx(iso)->pads[id];
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_gamepad_t>(iso, obj, gamepad, [](nx_gamepad_t *g) { free(g); });
	info.GetReturnValue().Set(obj);
}

void nx_gamepad_button_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	nx_gamepad_t *gamepad = nx::Unwrap<nx_gamepad_t>(info[0]);
	uint32_t index = 0;
	if (!info[1]->Uint32Value(context).To(&index))
		return;
	nx_gamepad_button_t *btn = static_cast<nx_gamepad_button_t *>(
	    calloc(1, sizeof(nx_gamepad_button_t)));
	btn->gamepad = gamepad;
	btn->mask = standard_button_masks[index];
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_gamepad_button_t>(iso, obj, btn,
	                              [](nx_gamepad_button_t *b) { free(b); });
	info.GetReturnValue().Set(obj);
}

void nx_gamepad_get_axes(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	nx_gamepad_t *gamepad = nx::Unwrap<nx_gamepad_t>(info.This());
	HidAnalogStickState l = padGetStickPos(gamepad->pad, 0);
	HidAnalogStickState r = padGetStickPos(gamepad->pad, 1);
	Local<Array> arr = Array::New(iso, 4);
	arr->Set(context, 0, Number::New(iso, (double)l.x / 32768.0)).Check();
	arr->Set(context, 1, Number::New(iso, (double)-l.y / 32768.0)).Check();
	arr->Set(context, 2, Number::New(iso, (double)r.x / 32768.0)).Check();
	arr->Set(context, 3, Number::New(iso, (double)-r.y / 32768.0)).Check();
	info.GetReturnValue().Set(arr);
}

void nx_gamepad_get_id(const FunctionCallbackInfo<Value> &info) {
	// Return an id UNIQUE per controller index. Previously this returned ""
	// for every pad, so all 8 controllers were indistinguishable — code that
	// keys state/config/slot-assignment by `gamepad.id` (the standard Web
	// Gamepad pattern) collapsed all pads onto one entry. The Web spec says
	// `id` should identify the controller; index is the stable discriminator
	// libnx gives us (HidNpadIdType 0..7).
	Isolate *iso = info.GetIsolate();
	nx_gamepad_t *gamepad = nx::Unwrap<nx_gamepad_t>(info.This());
	char buf[32];
	snprintf(buf, sizeof(buf), "switch-gamepad-%u", (unsigned)gamepad->id);
	info.GetReturnValue().Set(nx_str(iso, buf));
}

void nx_gamepad_get_raw_buttons(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_gamepad_t *gamepad = nx::Unwrap<nx_gamepad_t>(info.This());
	info.GetReturnValue().Set(
	    BigInt::NewFromUnsigned(iso, padGetButtons(gamepad->pad)));
}

void nx_gamepad_get_device_type(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_gamepad_t *gamepad = nx::Unwrap<nx_gamepad_t>(info.This());
	info.GetReturnValue().Set(
	    Integer::NewFromUnsigned(iso, hidGetNpadDeviceType(gamepad->id)));
}

void nx_gamepad_get_style_set(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_gamepad_t *gamepad = nx::Unwrap<nx_gamepad_t>(info.This());
	info.GetReturnValue().Set(
	    Integer::NewFromUnsigned(iso, padGetStyleSet(gamepad->pad)));
}

void nx_gamepad_get_index(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_gamepad_t *gamepad = nx::Unwrap<nx_gamepad_t>(info.This());
	info.GetReturnValue().Set(Integer::NewFromUnsigned(iso, gamepad->id));
}

void nx_gamepad_get_connected(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_gamepad_t *gamepad = nx::Unwrap<nx_gamepad_t>(info.This());
	info.GetReturnValue().Set(Boolean::New(iso, padIsConnected(gamepad->pad)));
}

bool button_pressed(Local<Value> this_val) {
	nx_gamepad_button_t *b = nx::Unwrap<nx_gamepad_button_t>(this_val);
	return (padGetButtons(b->gamepad->pad) & b->mask) != 0;
}

void nx_gamepad_button_get_pressed(const FunctionCallbackInfo<Value> &info) {
	info.GetReturnValue().Set(
	    Boolean::New(info.GetIsolate(), button_pressed(info.This())));
}

void nx_gamepad_button_get_touched(const FunctionCallbackInfo<Value> &info) {
	info.GetReturnValue().Set(
	    Boolean::New(info.GetIsolate(), button_pressed(info.This())));
}

void nx_gamepad_button_get_value(const FunctionCallbackInfo<Value> &info) {
	info.GetReturnValue().Set(
	    Integer::New(info.GetIsolate(), button_pressed(info.This()) ? 1 : 0));
}

Local<Object> get_proto(Isolate *iso, const FunctionCallbackInfo<Value> &info) {
	Local<Context> context = iso->GetCurrentContext();
	return info[0]
	    .As<Object>()
	    ->Get(context, nx_str(iso, "prototype"))
	    .ToLocalChecked()
	    .As<Object>();
}

void nx_gamepad_init_class(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Object> proto = get_proto(iso, info);
	NX_DEF_GET(proto, "axes", nx_gamepad_get_axes);
	NX_DEF_GET(proto, "id", nx_gamepad_get_id);
	NX_DEF_GET(proto, "index", nx_gamepad_get_index);
	NX_DEF_GET(proto, "connected", nx_gamepad_get_connected);
	NX_DEF_GET(proto, "deviceType", nx_gamepad_get_device_type);
	NX_DEF_GET(proto, "rawButtons", nx_gamepad_get_raw_buttons);
	NX_DEF_GET(proto, "styleSet", nx_gamepad_get_style_set);
}

void nx_gamepad_button_init_class(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Object> proto = get_proto(iso, info);
	NX_DEF_GET(proto, "pressed", nx_gamepad_button_get_pressed);
	NX_DEF_GET(proto, "touched", nx_gamepad_button_get_touched);
	NX_DEF_GET(proto, "value", nx_gamepad_button_get_value);
}

} // namespace

void nx_init_gamepad(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "gamepadInit", nx_gamepad_init_class);
	NX_SET_FUNC(init_obj, "gamepadNew", nx_gamepad_new);
	NX_SET_FUNC(init_obj, "gamepadButtonInit", nx_gamepad_button_init_class);
	NX_SET_FUNC(init_obj, "gamepadButtonNew", nx_gamepad_button_new);
}
