#include "error.h"
#include "types.h"
#include "wrap.h"
#include <string.h>

using namespace v8;

namespace {

typedef enum {
	WEB_MODE_NONE,
	WEB_MODE_WEB_SESSION,
	WEB_MODE_OFFLINE,
} nx_web_mode_t;

typedef struct {
	bool started;
	nx_web_mode_t mode;
	WebCommonConfig config;
	WebSession session;
	Event *exit_event;
} nx_web_applet_t;

void free_web_applet(nx_web_applet_t *data) {
	if (data->started) {
		WebCommonReply reply;
		webSessionWaitForExit(&data->session, &reply);
		webSessionClose(&data->session);
	}
	free(data);
}

void nx_web_applet_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Object> obj = nx::NewWrapped(iso);
	nx_web_applet_t *data =
	    static_cast<nx_web_applet_t *>(calloc(1, sizeof(nx_web_applet_t)));
	data->mode = WEB_MODE_NONE;
	nx::Wrap<nx_web_applet_t>(iso, obj, data, free_web_applet);
	info.GetReturnValue().Set(obj);
}

// Option readers off a JS options object.
int get_bool_opt(Isolate *iso, Local<Object> opts, const char *name) {
	Local<Context> context = iso->GetCurrentContext();
	Local<Value> v;
	if (!opts->Get(context, nx_str(iso, name)).ToLocal(&v) ||
	    v->IsUndefined()) {
		return -1;
	}
	return v->BooleanValue(iso) ? 1 : 0;
}

double get_float_opt(Isolate *iso, Local<Object> opts, const char *name) {
	Local<Context> context = iso->GetCurrentContext();
	Local<Value> v;
	if (!opts->Get(context, nx_str(iso, name)).ToLocal(&v) ||
	    v->IsUndefined()) {
		return -1.0;
	}
	double d = -1.0;
	if (!v->NumberValue(context).To(&d))
		return -1.0;
	return d;
}

// Returns -1 if absent; otherwise the matching enum value.
int get_enum_opt(Isolate *iso, Local<Object> opts, const char *name,
                 const char **names, const int *values, int count) {
	Local<Context> context = iso->GetCurrentContext();
	Local<Value> v;
	if (!opts->Get(context, nx_str(iso, name)).ToLocal(&v) ||
	    v->IsUndefined() || v->IsNull()) {
		return -1;
	}
	String::Utf8Value str(iso, v);
	if (!*str)
		return -1;
	for (int i = 0; i < count; i++) {
		if (strcmp(*str, names[i]) == 0)
			return values[i];
	}
	return -1;
}

bool is_htmldoc_url(const char *url) {
	return url && strncmp(url, "htmldoc:", 8) == 0;
}

void apply_options(Isolate *iso, nx_web_applet_t *data, Local<Object> opts) {
	int v;
	double fv;
	v = get_bool_opt(iso, opts, "jsExtension");
	if (v >= 0)
		webConfigSetJsExtension(&data->config, v);
	v = get_bool_opt(iso, opts, "bootHidden");
	if (v > 0)
		webConfigSetBootMode(
		    &data->config,
		    WebSessionBootMode_AllForegroundInitiallyHidden);
	static const char *boot_display_names[] = {"default", "white", "black"};
	static const int boot_display_values[] = {0, 1, 2};
	v = get_enum_opt(iso, opts, "bootDisplayKind", boot_display_names,
	                 boot_display_values, 3);
	if (v >= 0)
		webConfigSetBootDisplayKind(&data->config, (WebBootDisplayKind)v);
	static const char *bg_names[] = {"default"};
	static const int bg_values[] = {0};
	v = get_enum_opt(iso, opts, "backgroundKind", bg_names, bg_values, 1);
	if (v >= 0)
		webConfigSetBackgroundKind(&data->config, (WebBackgroundKind)v);
	v = get_bool_opt(iso, opts, "footer");
	if (v >= 0)
		webConfigSetFooter(&data->config, v);
	v = get_bool_opt(iso, opts, "pointer");
	if (v >= 0)
		webConfigSetPointer(&data->config, v);
	static const char *stick_names[] = {"pointer", "cursor"};
	static const int stick_values[] = {0, 1};
	v = get_enum_opt(iso, opts, "leftStickMode", stick_names, stick_values, 2);
	if (v >= 0)
		webConfigSetLeftStickMode(&data->config, (WebLeftStickMode)v);
	v = get_bool_opt(iso, opts, "bootAsMediaPlayer");
	if (v >= 0)
		webConfigSetBootAsMediaPlayer(&data->config, v);
	v = get_bool_opt(iso, opts, "screenShot");
	if (v >= 0)
		webConfigSetScreenShot(&data->config, v);
	v = get_bool_opt(iso, opts, "pageCache");
	if (v >= 0)
		webConfigSetPageCache(&data->config, v);
	v = get_bool_opt(iso, opts, "webAudio");
	if (v >= 0)
		webConfigSetWebAudio(&data->config, v);
	static const char *footer_fixed_names[] = {"default", "always", "hidden"};
	static const int footer_fixed_values[] = {0, 1, 2};
	v = get_enum_opt(iso, opts, "footerFixedKind", footer_fixed_names,
	                 footer_fixed_values, 3);
	if (v >= 0)
		webConfigSetFooterFixedKind(&data->config, (WebFooterFixedKind)v);
	v = get_bool_opt(iso, opts, "pageFade");
	if (v >= 0)
		webConfigSetPageFade(&data->config, v);
	v = get_bool_opt(iso, opts, "bootLoadingIcon");
	if (v >= 0)
		webConfigSetBootLoadingIcon(&data->config, v);
	v = get_bool_opt(iso, opts, "pageScrollIndicator");
	if (v >= 0)
		webConfigSetPageScrollIndicator(&data->config, v);
	v = get_bool_opt(iso, opts, "mediaPlayerSpeedControl");
	if (v >= 0)
		webConfigSetMediaPlayerSpeedControl(&data->config, v);
	v = get_bool_opt(iso, opts, "mediaAutoPlay");
	if (v >= 0)
		webConfigSetMediaAutoPlay(&data->config, v);
	fv = get_float_opt(iso, opts, "overrideWebAudioVolume");
	if (fv >= 0.0)
		webConfigSetOverrideWebAudioVolume(&data->config, (float)fv);
	fv = get_float_opt(iso, opts, "overrideMediaAudioVolume");
	if (fv >= 0.0)
		webConfigSetOverrideMediaAudioVolume(&data->config, (float)fv);
	v = get_bool_opt(iso, opts, "mediaPlayerAutoClose");
	if (v >= 0)
		webConfigSetMediaPlayerAutoClose(&data->config, v);
	v = get_bool_opt(iso, opts, "mediaPlayerUi");
	if (v >= 0)
		webConfigSetMediaPlayerUi(&data->config, v);
	{
		Local<Context> context = iso->GetCurrentContext();
		Local<Value> ua;
		if (opts->Get(context, nx_str(iso, "userAgentAdditionalString"))
		        .ToLocal(&ua) &&
		    !ua->IsUndefined() && !ua->IsNull()) {
			String::Utf8Value uas(iso, ua);
			if (*uas)
				webConfigSetUserAgentAdditionalString(&data->config, *uas);
		}
	}
	webConfigSetTouchEnabledOnContents(&data->config, true);
}

Result start_web_session(Isolate *iso, nx_web_applet_t *data, const char *url,
                         Local<Object> opts) {
	Result rc = webPageCreate(&data->config, url);
	if (R_FAILED(rc))
		return rc;
	rc = webConfigSetWhitelist(&data->config, "^http://.*$\n^https://.*$");
	if (R_FAILED(rc))
		return rc;
	apply_options(iso, data, opts);
	webSessionCreate(&data->session, &data->config);
	rc = webSessionStart(&data->session, &data->exit_event);
	if (R_FAILED(rc)) {
		webSessionClose(&data->session);
		return rc;
	}
	data->mode = WEB_MODE_WEB_SESSION;
	return 0;
}

Result start_htmldoc(Isolate *iso, nx_web_applet_t *data, const char *url,
                     Local<Object> opts) {
	const char *user_path = url + 8;
	if (*user_path == '/')
		user_path++;
	size_t prefix_len = 8; // ".htdocs/"
	size_t user_len = strlen(user_path);
	char *doc_path = (char *)malloc(prefix_len + user_len + 1);
	if (!doc_path)
		return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
	memcpy(doc_path, ".htdocs/", prefix_len);
	memcpy(doc_path + prefix_len, user_path, user_len + 1);
	Result rc = webOfflineCreate(&data->config,
	                             WebDocumentKind_OfflineHtmlPage, 0, doc_path);
	free(doc_path);
	if (R_FAILED(rc))
		return rc;
	apply_options(iso, data, opts);
	webSessionCreate(&data->session, &data->config);
	rc = webSessionStart(&data->session, &data->exit_event);
	if (R_FAILED(rc)) {
		webSessionClose(&data->session);
		return rc;
	}
	data->mode = WEB_MODE_OFFLINE;
	return 0;
}

void nx_web_applet_start(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_web_applet_t *data = nx::Unwrap<nx_web_applet_t>(info[0]);
	if (!data)
		return;
	if (data->started) {
		nx_throw(iso, "WebApplet already started");
		return;
	}
	String::Utf8Value url(iso, info[1]);
	if (!*url)
		return;
	Local<Object> opts = info[2].As<Object>();
	AppletType at = appletGetAppletType();
	if (at != AppletType_Application && at != AppletType_SystemApplication) {
		nx_throw(iso, "WebApplet requires Application mode. Launch via NSP or "
		              "hold R when opening a game to use hbmenu in Application "
		              "mode.");
		return;
	}
	Result rc;
	if (is_htmldoc_url(*url)) {
		rc = start_htmldoc(iso, data, *url, opts);
	} else {
		rc = start_web_session(iso, data, *url, opts);
	}
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "WebApplet start");
		return;
	}
	data->started = true;
}

void nx_web_applet_appear(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_web_applet_t *data = nx::Unwrap<nx_web_applet_t>(info[0]);
	if (!data)
		return;
	if (!data->started) {
		nx_throw(iso, "WebApplet not started");
		return;
	}
	bool flag = false;
	Result rc = webSessionAppear(&data->session, &flag);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "webSessionAppear");
		return;
	}
	info.GetReturnValue().Set(flag);
}

void nx_web_applet_send_message(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_web_applet_t *data = nx::Unwrap<nx_web_applet_t>(info[0]);
	if (!data)
		return;
	if (!data->started) {
		nx_throw(iso, "WebApplet not started");
		return;
	}
	String::Utf8Value msg(iso, info[1]);
	if (!*msg)
		return;
	bool flag = false;
	Result rc = webSessionTrySendContentMessage(
	    &data->session, *msg, (u32)(msg.length() + 1), &flag);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "webSessionTrySendContentMessage");
		return;
	}
	info.GetReturnValue().Set(flag);
}

void nx_web_applet_poll_messages(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	nx_web_applet_t *data = nx::Unwrap<nx_web_applet_t>(info[0]);
	Local<Array> arr = Array::New(iso, 0);
	if (!data || !data->started) {
		info.GetReturnValue().Set(arr);
		return;
	}
	uint32_t index = 0;
	char buf[0x2000];
	while (1) {
		u64 out_size = 0;
		bool flag = false;
		Result rc = webSessionTryReceiveContentMessage(
		    &data->session, buf, sizeof(buf), &out_size, &flag);
		if (R_FAILED(rc) || !flag)
			break;
		size_t str_len = out_size > 0 ? strnlen(buf, (size_t)out_size) : 0;
		arr->Set(context, index++,
		         String::NewFromUtf8(iso, buf, NewStringType::kNormal,
		                             (int)str_len)
		             .ToLocalChecked())
		    .Check();
	}
	info.GetReturnValue().Set(arr);
}

void nx_web_applet_request_exit(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_web_applet_t *data = nx::Unwrap<nx_web_applet_t>(info[0]);
	if (!data)
		return;
	if (!data->started) {
		nx_throw(iso, "WebApplet not started");
		return;
	}
	Result rc = webSessionRequestExit(&data->session);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "requestExit");
	}
}

void nx_web_applet_close(const FunctionCallbackInfo<Value> &info) {
	nx_web_applet_t *data = nx::Unwrap<nx_web_applet_t>(info[0]);
	if (data && data->started) {
		WebCommonReply reply;
		webSessionWaitForExit(&data->session, &reply);
		webSessionClose(&data->session);
		data->started = false;
		data->mode = WEB_MODE_NONE;
	}
}

void nx_web_applet_is_running(const FunctionCallbackInfo<Value> &info) {
	nx_web_applet_t *data = nx::Unwrap<nx_web_applet_t>(info[0]);
	if (!data || !data->started) {
		info.GetReturnValue().Set(false);
		return;
	}
	if (data->exit_event) {
		Result rc = eventWait(data->exit_event, 0);
		if (R_SUCCEEDED(rc)) {
			info.GetReturnValue().Set(false);
			return;
		}
	}
	info.GetReturnValue().Set(true);
}

void nx_web_applet_get_mode(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_web_applet_t *data = nx::Unwrap<nx_web_applet_t>(info[0]);
	const char *mode = "none";
	if (data) {
		if (data->mode == WEB_MODE_WEB_SESSION)
			mode = "web-session";
		else if (data->mode == WEB_MODE_OFFLINE)
			mode = "htmldoc";
	}
	info.GetReturnValue().Set(nx_str(iso, mode));
}

} // namespace

void nx_init_web(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "webAppletNew", nx_web_applet_new);
	NX_SET_FUNC(init_obj, "webAppletStart", nx_web_applet_start);
	NX_SET_FUNC(init_obj, "webAppletAppear", nx_web_applet_appear);
	NX_SET_FUNC(init_obj, "webAppletSendMessage", nx_web_applet_send_message);
	NX_SET_FUNC(init_obj, "webAppletPollMessages", nx_web_applet_poll_messages);
	NX_SET_FUNC(init_obj, "webAppletRequestExit", nx_web_applet_request_exit);
	NX_SET_FUNC(init_obj, "webAppletClose", nx_web_applet_close);
	NX_SET_FUNC(init_obj, "webAppletIsRunning", nx_web_applet_is_running);
	NX_SET_FUNC(init_obj, "webAppletGetMode", nx_web_applet_get_mode);
}
