#include "error.h"
#include "types.h"
#include "wrap.h"

using namespace v8;

namespace {

typedef struct {
	SwkbdInline kbdinline;
	SwkbdAppearArg appearArg;
	Isolate *iso;
	Global<Object> instance;
	Global<Function> cancel_func;
	Global<Function> change_func;
	Global<Function> submit_func;
	Global<Function> cursor_move_func;
} nx_swkbd_t;

// The libnx swkbd callbacks are global C functions, so the active keyboard is
// tracked in a file-global pointer (only one inline keyboard at a time).
static nx_swkbd_t *current_kbd;

nx_swkbd_t *get_kbd(Local<Value> v) { return nx::Unwrap<nx_swkbd_t>(v); }

void free_kbd(nx_swkbd_t *data) {
	swkbdInlineClose(&data->kbdinline);
	data->instance.Reset();
	data->cancel_func.Reset();
	data->change_func.Reset();
	data->submit_func.Reset();
	data->cursor_move_func.Reset();
	free(data);
}

// Invoke a retained callback with args (main thread; V8 OK).
void call_cb(Global<Function> &g, int argc, Local<Value> *argv) {
	if (!current_kbd || g.IsEmpty())
		return;
	Isolate *iso = current_kbd->iso;
	HandleScope scope(iso);
	Local<Context> ctx = iso->GetCurrentContext();
	Local<Function> fn = g.Get(iso);
	Local<Object> recv = current_kbd->instance.Get(iso);
	TryCatch tc(iso);
	Local<Value> ret;
	if (!fn->Call(ctx, recv, argc, argv).ToLocal(&ret)) {
		nx_emit_error_event(iso, &tc);
	}
}

void finishinit_cb(void) {}

void decidedcancel_cb(void) {
	if (!current_kbd)
		return;
	call_cb(current_kbd->cancel_func, 0, nullptr);
	current_kbd = NULL;
}

void strchange_cb(const char *str, SwkbdChangedStringArg *arg) {
	if (!current_kbd)
		return;
	Isolate *iso = current_kbd->iso;
	HandleScope scope(iso);
	Local<Value> args[] = {nx_str(iso, str), Integer::New(iso, arg->cursorPos),
	                       Integer::New(iso, arg->dicStartCursorPos),
	                       Integer::New(iso, arg->dicEndCursorPos)};
	call_cb(current_kbd->change_func, 4, args);
}

void movedcursor_cb(const char *str, SwkbdMovedCursorArg *arg) {
	if (!current_kbd)
		return;
	Isolate *iso = current_kbd->iso;
	HandleScope scope(iso);
	Local<Value> args[] = {nx_str(iso, str), Integer::New(iso, arg->cursorPos)};
	call_cb(current_kbd->cursor_move_func, 2, args);
}

void decidedenter_cb(const char *str, SwkbdDecidedEnterArg *arg) {
	if (!current_kbd)
		return;
	(void)arg;
	Isolate *iso = current_kbd->iso;
	HandleScope scope(iso);
	Local<Value> args[] = {nx_str(iso, str)};
	call_cb(current_kbd->submit_func, 1, args);
	current_kbd = NULL;
}

// Read an optional function property off an options object.
void set_fn(Isolate *iso, Local<Object> opts, const char *key,
            Global<Function> &out) {
	Local<Context> ctx = iso->GetCurrentContext();
	Local<Value> v;
	if (opts->Get(ctx, nx_str(iso, key)).ToLocal(&v) && v->IsFunction())
		out.Reset(iso, v.As<Function>());
}

void nx_swkbd_create(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Object> opts = info[0].As<Object>();
	Local<Object> obj = nx::NewWrapped(iso);
	nx_swkbd_t *data = (nx_swkbd_t *)calloc(1, sizeof(nx_swkbd_t));
	data->iso = iso;
	data->instance.Reset(iso, obj);
	set_fn(iso, opts, "onCancel", data->cancel_func);
	set_fn(iso, opts, "onChange", data->change_func);
	set_fn(iso, opts, "onSubmit", data->submit_func);
	set_fn(iso, opts, "onCursorMove", data->cursor_move_func);
	nx::Wrap<nx_swkbd_t>(iso, obj, data, free_kbd);

	Result rc = swkbdInlineCreate(&data->kbdinline);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "swkbdInlineCreate");
		return;
	}
	swkbdInlineSetFinishedInitializeCallback(&data->kbdinline, finishinit_cb);
	rc = swkbdInlineLaunchForLibraryApplet(&data->kbdinline,
	                                       SwkbdInlineMode_AppletDisplay, 0);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "swkbdInlineLaunchForLibraryApplet");
		return;
	}
	swkbdInlineSetChangedStringCallback(&data->kbdinline, strchange_cb);
	swkbdInlineSetMovedCursorCallback(&data->kbdinline, movedcursor_cb);
	swkbdInlineSetDecidedEnterCallback(&data->kbdinline, decidedenter_cb);
	swkbdInlineSetDecidedCancelCallback(&data->kbdinline, decidedcancel_cb);
	swkbdInlineMakeAppearArg(&data->appearArg, SwkbdType_Normal);
	info.GetReturnValue().Set(obj);
}

int opt_int(Isolate *iso, Local<Object> o, const char *key, int def) {
	Local<Context> ctx = iso->GetCurrentContext();
	Local<Value> v;
	if (o->Get(ctx, nx_str(iso, key)).ToLocal(&v) && v->IsNumber()) {
		int32_t n;
		if (v->Int32Value(ctx).To(&n))
			return n;
	}
	return def;
}
bool opt_bool(Isolate *iso, Local<Object> o, const char *key) {
	Local<Context> ctx = iso->GetCurrentContext();
	Local<Value> v;
	if (o->Get(ctx, nx_str(iso, key)).ToLocal(&v))
		return v->BooleanValue(iso);
	return false;
}
void opt_button(Isolate *iso, Local<Object> o, const char *key,
                void (*setter)(SwkbdAppearArg *, const char *),
                SwkbdAppearArg *arg) {
	Local<Context> ctx = iso->GetCurrentContext();
	Local<Value> v;
	if (o->Get(ctx, nx_str(iso, key)).ToLocal(&v) && v->IsString()) {
		String::Utf8Value s(iso, v);
		if (*s)
			setter(arg, *s);
	}
}

void nx_swkbd_show(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> ctx = iso->GetCurrentContext();
	nx_swkbd_t *data = get_kbd(info[0]);
	if (!data)
		return;
	current_kbd = data;
	Local<Object> o = info[0].As<Object>();
	data->appearArg.type = (SwkbdType)opt_int(iso, o, "type", SwkbdType_Normal);
	opt_button(iso, o, "okButtonText", swkbdInlineAppearArgSetOkButtonText,
	           &data->appearArg);
	opt_button(iso, o, "leftButtonText", swkbdInlineAppearArgSetLeftButtonText,
	           &data->appearArg);
	opt_button(iso, o, "rightButtonText",
	           swkbdInlineAppearArgSetRightButtonText, &data->appearArg);
	data->appearArg.dicFlag = opt_bool(iso, o, "enableDictionary");
	data->appearArg.returnButtonFlag = opt_bool(iso, o, "enableReturn");
	data->appearArg.stringLenMin = opt_int(iso, o, "minLength", 0);
	data->appearArg.stringLenMax = opt_int(iso, o, "maxLength", 0);
	swkbdInlineAppear(&data->kbdinline, &data->appearArg);

	int x = 0, y = 0, width = 0, height = 0;
	SwkbdRect keytop, footer;
	int n = swkbdInlineGetTouchRectangles(&data->kbdinline, &keytop, &footer);
	if (n >= 1) {
		x += keytop.x; y += keytop.y;
		width += keytop.width; height += keytop.height;
	}
	if (n >= 2) {
		x += footer.x; y += footer.y;
		width += footer.width; height += footer.height;
	}
	Local<Object> dims = Object::New(iso);
	dims->Set(ctx, nx_str(iso, "x"), Integer::New(iso, x)).Check();
	dims->Set(ctx, nx_str(iso, "y"), Integer::New(iso, y)).Check();
	dims->Set(ctx, nx_str(iso, "width"), Integer::New(iso, width)).Check();
	dims->Set(ctx, nx_str(iso, "height"), Integer::New(iso, height)).Check();
	info.GetReturnValue().Set(dims);
}

void nx_swkbd_hide(const FunctionCallbackInfo<Value> &info) {
	nx_swkbd_t *data = get_kbd(info[0]);
	if (!data)
		return;
	current_kbd = NULL;
	swkbdInlineDisappear(&data->kbdinline);
}

void nx_swkbd_update(const FunctionCallbackInfo<Value> &info) {
	nx_swkbd_t *data = get_kbd(info.This());
	if (data)
		swkbdInlineUpdate(&data->kbdinline, NULL);
}

void nx_swkbd_set_cursor_pos(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_swkbd_t *data = get_kbd(info[0]);
	if (!data)
		return;
	int32_t pos;
	if (!info[1]->Int32Value(iso->GetCurrentContext()).To(&pos))
		return;
	swkbdInlineSetCursorPos(&data->kbdinline, pos);
}

void nx_swkbd_set_input_text(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_swkbd_t *data = get_kbd(info[0]);
	if (!data)
		return;
	String::Utf8Value value(iso, info[1]);
	if (*value)
		swkbdInlineSetInputText(&data->kbdinline, *value);
}

} // namespace

void nx_init_swkbd(Isolate *iso, Local<Object> init_obj) {
	current_kbd = NULL;
	NX_SET_FUNC(init_obj, "swkbdCreate", nx_swkbd_create);
	NX_SET_FUNC(init_obj, "swkbdHide", nx_swkbd_hide);
	NX_SET_FUNC(init_obj, "swkbdShow", nx_swkbd_show);
	NX_SET_FUNC(init_obj, "swkbdSetCursorPos", nx_swkbd_set_cursor_pos);
	NX_SET_FUNC(init_obj, "swkbdSetInputText", nx_swkbd_set_input_text);
	NX_SET_FUNC(init_obj, "swkbdUpdate", nx_swkbd_update);
}
