#include "web.h"
#include "error.h"
#include <string.h>

static JSClassID nx_web_applet_class_id;

typedef enum {
	WEB_MODE_NONE,
	WEB_MODE_WEB_SESSION,   // WebApplet with WebSession (HTTP/HTTPS, window.nx)
	WEB_MODE_OFFLINE,       // Offline applet (html-document NCA, window.nx)
} nx_web_mode_t;

typedef struct {
	bool started;
	nx_web_mode_t mode;
	JSContext *ctx;

	WebCommonConfig config;
	WebSession session;
	Event *exit_event;
} nx_web_applet_t;

static nx_web_applet_t *nx_web_applet_get(JSContext *ctx, JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_web_applet_class_id);
}

static void finalizer_web_applet(JSRuntime *rt, JSValue val) {
	nx_web_applet_t *data = JS_GetOpaque(val, nx_web_applet_class_id);
	if (data) {
		if (data->started) {
			WebCommonReply reply;
			webSessionWaitForExit(&data->session, &reply);
			webSessionClose(&data->session);
		}
		js_free_rt(rt, data);
	}
}

static JSValue nx_web_applet_new(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	JSValue obj = JS_NewObjectClass(ctx, nx_web_applet_class_id);
	nx_web_applet_t *data = js_mallocz(ctx, sizeof(nx_web_applet_t));
	if (!data) {
		return JS_EXCEPTION;
	}
	JS_SetOpaque(obj, data);
	data->ctx = ctx;
	data->started = false;
	data->mode = WEB_MODE_NONE;
	return obj;
}

// Helper: get a boolean property from the options object, returns -1 if not present
static int _get_bool_opt(JSContext *ctx, JSValueConst opts, const char *name) {
	JSValue val = JS_GetPropertyStr(ctx, opts, name);
	if (JS_IsUndefined(val)) return -1;
	int result = JS_ToBool(ctx, val);
	JS_FreeValue(ctx, val);
	return result;
}

// Helper: get an int property from the options object, returns -1 if not present
static int _get_int_opt(JSContext *ctx, JSValueConst opts, const char *name) {
	JSValue val = JS_GetPropertyStr(ctx, opts, name);
	if (JS_IsUndefined(val)) return -1;
	int32_t result;
	JS_ToInt32(ctx, &result, val);
	JS_FreeValue(ctx, val);
	return result;
}

// Helper: get a double property from the options object, returns -1.0 if not present
static double _get_float_opt(JSContext *ctx, JSValueConst opts, const char *name) {
	JSValue val = JS_GetPropertyStr(ctx, opts, name);
	if (JS_IsUndefined(val)) return -1.0;
	double result;
	JS_ToFloat64(ctx, &result, val);
	JS_FreeValue(ctx, val);
	return result;
}

// Helper: get a string property from the options object. Returns NULL if not present.
// Caller must call JS_FreeCString on non-NULL result.
static const char *_get_string_opt(JSContext *ctx, JSValueConst opts, const char *name) {
	JSValue val = JS_GetPropertyStr(ctx, opts, name);
	if (JS_IsUndefined(val) || JS_IsNull(val)) {
		JS_FreeValue(ctx, val);
		return NULL;
	}
	const char *str = JS_ToCString(ctx, val);
	JS_FreeValue(ctx, val);
	return str;
}

// Helper: get a string enum as int. Maps string values to ints. Returns -1 if not present.
static int _get_enum_opt(JSContext *ctx, JSValueConst opts, const char *name,
						 const char **names, const int *values, int count) {
	const char *str = _get_string_opt(ctx, opts, name);
	if (!str) return -1;
	for (int i = 0; i < count; i++) {
		if (strcmp(str, names[i]) == 0) {
			JS_FreeCString(ctx, str);
			return values[i];
		}
	}
	JS_FreeCString(ctx, str);
	return -1;
}

static bool _is_htmldoc_url(const char *url) {
	return url && strncmp(url, "htmldoc:", 8) == 0;
}

// Apply options from JS object to the WebCommonConfig
static void _apply_options(JSContext *ctx, nx_web_applet_t *data, JSValueConst opts) {
	int v;
	double fv;

	v = _get_bool_opt(ctx, opts, "jsExtension");
	if (v >= 0) webConfigSetJsExtension(&data->config, v);

	v = _get_bool_opt(ctx, opts, "bootHidden");
	if (v > 0) webConfigSetBootMode(&data->config, WebSessionBootMode_AllForegroundInitiallyHidden);

	static const char *boot_display_names[] = { "default", "white", "black" };
	static const int boot_display_values[] = { 0, 1, 2 };
	v = _get_enum_opt(ctx, opts, "bootDisplayKind", boot_display_names, boot_display_values, 3);
	if (v >= 0) webConfigSetBootDisplayKind(&data->config, v);

	static const char *bg_names[] = { "default" };
	static const int bg_values[] = { 0 };
	v = _get_enum_opt(ctx, opts, "backgroundKind", bg_names, bg_values, 1);
	if (v >= 0) webConfigSetBackgroundKind(&data->config, v);

	v = _get_bool_opt(ctx, opts, "footer");
	if (v >= 0) webConfigSetFooter(&data->config, v);

	v = _get_bool_opt(ctx, opts, "pointer");
	if (v >= 0) webConfigSetPointer(&data->config, v);

	static const char *stick_names[] = { "pointer", "cursor" };
	static const int stick_values[] = { 0, 1 };
	v = _get_enum_opt(ctx, opts, "leftStickMode", stick_names, stick_values, 2);
	if (v >= 0) webConfigSetLeftStickMode(&data->config, v);

	v = _get_bool_opt(ctx, opts, "bootAsMediaPlayer");
	if (v >= 0) webConfigSetBootAsMediaPlayer(&data->config, v);

	v = _get_bool_opt(ctx, opts, "screenShot");
	if (v >= 0) webConfigSetScreenShot(&data->config, v);

	v = _get_bool_opt(ctx, opts, "pageCache");
	if (v >= 0) webConfigSetPageCache(&data->config, v);

	v = _get_bool_opt(ctx, opts, "webAudio");
	if (v >= 0) webConfigSetWebAudio(&data->config, v);

	static const char *footer_fixed_names[] = { "default", "always", "hidden" };
	static const int footer_fixed_values[] = { 0, 1, 2 };
	v = _get_enum_opt(ctx, opts, "footerFixedKind", footer_fixed_names, footer_fixed_values, 3);
	if (v >= 0) webConfigSetFooterFixedKind(&data->config, v);

	v = _get_bool_opt(ctx, opts, "pageFade");
	if (v >= 0) webConfigSetPageFade(&data->config, v);

	v = _get_bool_opt(ctx, opts, "bootLoadingIcon");
	if (v >= 0) webConfigSetBootLoadingIcon(&data->config, v);

	v = _get_bool_opt(ctx, opts, "pageScrollIndicator");
	if (v >= 0) webConfigSetPageScrollIndicator(&data->config, v);

	v = _get_bool_opt(ctx, opts, "mediaPlayerSpeedControl");
	if (v >= 0) webConfigSetMediaPlayerSpeedControl(&data->config, v);

	v = _get_bool_opt(ctx, opts, "mediaAutoPlay");
	if (v >= 0) webConfigSetMediaAutoPlay(&data->config, v);

	fv = _get_float_opt(ctx, opts, "overrideWebAudioVolume");
	if (fv >= 0.0) webConfigSetOverrideWebAudioVolume(&data->config, (float)fv);

	fv = _get_float_opt(ctx, opts, "overrideMediaAudioVolume");
	if (fv >= 0.0) webConfigSetOverrideMediaAudioVolume(&data->config, (float)fv);

	v = _get_bool_opt(ctx, opts, "mediaPlayerAutoClose");
	if (v >= 0) webConfigSetMediaPlayerAutoClose(&data->config, v);

	v = _get_bool_opt(ctx, opts, "mediaPlayerUi");
	if (v >= 0) webConfigSetMediaPlayerUi(&data->config, v);

	const char *ua = _get_string_opt(ctx, opts, "userAgentAdditionalString");
	if (ua) {
		webConfigSetUserAgentAdditionalString(&data->config, ua);
		JS_FreeCString(ctx, ua);
	}

	webConfigSetTouchEnabledOnContents(&data->config, true);
}

static Result _start_web_session(nx_web_applet_t *data, const char *url,
								 JSContext *ctx, JSValueConst opts) {
	Result rc;

	rc = webPageCreate(&data->config, url);
	if (R_FAILED(rc)) return rc;

	rc = webConfigSetWhitelist(&data->config, "^http://.*$\n^https://.*$");
	if (R_FAILED(rc)) return rc;

	_apply_options(ctx, data, opts);

	webSessionCreate(&data->session, &data->config);
	rc = webSessionStart(&data->session, &data->exit_event);
	if (R_FAILED(rc)) {
		webSessionClose(&data->session);
		return rc;
	}

	data->mode = WEB_MODE_WEB_SESSION;
	return 0;
}

static Result _start_htmldoc(nx_web_applet_t *data, const char *url,
							 JSContext *ctx, JSValueConst opts) {
	Result rc;

	// DocumentPath: skip "htmldoc:" prefix (and optional '/') to get
	// a relative path. Per libnx/switchbrew docs:
	// - id=0 for OfflineHtmlPage (uses the calling application's content)
	// - Path is relative to "html-document/" in the HtmlDocument NCA RomFS
	// - Path must contain ".htdocs/"
	// - Path must not have a leading '/'
	const char *doc_path = url + 8;  // skip "htmldoc:"
	if (*doc_path == '/') doc_path++;  // skip optional '/'

	rc = webOfflineCreate(&data->config, WebDocumentKind_OfflineHtmlPage,
						  0, doc_path);
	if (R_FAILED(rc)) return rc;

	_apply_options(ctx, data, opts);

	// Use WebSession for async (offline ShimKind supports it on FW 7.0+)
	webSessionCreate(&data->session, &data->config);
	rc = webSessionStart(&data->session, &data->exit_event);
	if (R_FAILED(rc)) {
		webSessionClose(&data->session);
		return rc;
	}

	data->mode = WEB_MODE_OFFLINE;
	return 0;
}

// webAppletStart(applet, url, options)
static JSValue nx_web_applet_start(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_web_applet_t *data = nx_web_applet_get(ctx, argv[0]);
	if (!data) return JS_EXCEPTION;

	if (data->started) {
		return JS_ThrowTypeError(ctx, "WebApplet already started");
	}

	const char *url = JS_ToCString(ctx, argv[1]);
	if (!url) return JS_EXCEPTION;

	JSValueConst opts = argv[2];

	// Web applets can only be launched from Application mode
	AppletType at = appletGetAppletType();
	if (at != AppletType_Application && at != AppletType_SystemApplication) {
		JS_FreeCString(ctx, url);
		return JS_ThrowTypeError(ctx,
			"WebApplet requires Application mode. "
			"Launch via NSP or hold R when opening a game to use hbmenu "
			"in Application mode.");
	}

	Result rc;
	if (_is_htmldoc_url(url)) {
		rc = _start_htmldoc(data, url, ctx, opts);
	} else {
		rc = _start_web_session(data, url, ctx, opts);
	}

	JS_FreeCString(ctx, url);

	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "WebApplet start");
	}

	data->started = true;
	return JS_UNDEFINED;
}

static JSValue nx_web_applet_appear(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_web_applet_t *data = nx_web_applet_get(ctx, argv[0]);
	if (!data) return JS_EXCEPTION;
	if (!data->started) {
		return JS_ThrowTypeError(ctx, "WebApplet not started");
	}
	bool flag = false;
	Result rc = webSessionAppear(&data->session, &flag);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "webSessionAppear()");
	}
	return JS_NewBool(ctx, flag);
}

static JSValue nx_web_applet_send_message(JSContext *ctx,
										  JSValueConst this_val, int argc,
										  JSValueConst *argv) {
	nx_web_applet_t *data = nx_web_applet_get(ctx, argv[0]);
	if (!data) return JS_EXCEPTION;
	if (!data->started) {
		return JS_ThrowTypeError(ctx, "WebApplet not started");
	}
	size_t len;
	const char *msg = JS_ToCStringLen(ctx, &len, argv[1]);
	if (!msg) return JS_EXCEPTION;
	bool flag = false;
	// Send len+1 to include the NUL terminator. The browser-side
	// webSessionTryReceiveContentMessage NUL-terminates at size-1,
	// so without the extra byte the last character gets truncated.
	Result rc = webSessionTrySendContentMessage(&data->session, msg, (u32)(len + 1),
												&flag);
	JS_FreeCString(ctx, msg);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc,
									"webSessionTrySendContentMessage()");
	}
	return JS_NewBool(ctx, flag);
}

static JSValue nx_web_applet_poll_messages(JSContext *ctx,
										   JSValueConst this_val, int argc,
										   JSValueConst *argv) {
	nx_web_applet_t *data = nx_web_applet_get(ctx, argv[0]);
	if (!data) return JS_EXCEPTION;
	if (!data->started) {
		return JS_NewArray(ctx);
	}

	JSValue arr = JS_NewArray(ctx);
	u32 index = 0;
	char buf[0x2000];

	while (1) {
		u64 out_size = 0;
		bool flag = false;
		Result rc = webSessionTryReceiveContentMessage(
			&data->session, buf, sizeof(buf), &out_size, &flag);
		if (R_FAILED(rc) || !flag) {
			break;
		}
		size_t str_len = out_size > 0 ? strnlen(buf, (size_t)out_size) : 0;
		JSValue str = JS_NewStringLen(ctx, buf, str_len);
		JS_SetPropertyUint32(ctx, arr, index++, str);
	}

	return arr;
}

static JSValue nx_web_applet_request_exit(JSContext *ctx,
										  JSValueConst this_val, int argc,
										  JSValueConst *argv) {
	nx_web_applet_t *data = nx_web_applet_get(ctx, argv[0]);
	if (!data) return JS_EXCEPTION;
	if (!data->started) {
		return JS_ThrowTypeError(ctx, "WebApplet not started");
	}
	Result rc = webSessionRequestExit(&data->session);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "requestExit()");
	}
	return JS_UNDEFINED;
}

static JSValue nx_web_applet_close(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_web_applet_t *data = nx_web_applet_get(ctx, argv[0]);
	if (!data) return JS_EXCEPTION;
	if (data->started) {
		WebCommonReply reply;
		webSessionWaitForExit(&data->session, &reply);
		webSessionClose(&data->session);
		data->started = false;
		data->mode = WEB_MODE_NONE;
	}
	return JS_UNDEFINED;
}

static JSValue nx_web_applet_is_running(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	nx_web_applet_t *data = nx_web_applet_get(ctx, argv[0]);
	if (!data) return JS_EXCEPTION;
	if (!data->started) {
		return JS_FALSE;
	}

	if (data->exit_event) {
		Result rc = eventWait(data->exit_event, 0);
		if (R_SUCCEEDED(rc)) return JS_FALSE;
	}

	return JS_TRUE;
}

static JSValue nx_web_applet_get_mode(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_web_applet_t *data = nx_web_applet_get(ctx, argv[0]);
	if (!data) return JS_EXCEPTION;
	switch (data->mode) {
		case WEB_MODE_WEB_SESSION: return JS_NewString(ctx, "web-session");
		case WEB_MODE_OFFLINE: return JS_NewString(ctx, "htmldoc");
		default: return JS_NewString(ctx, "none");
	}
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("webAppletNew", 0, nx_web_applet_new),
	JS_CFUNC_DEF("webAppletStart", 3, nx_web_applet_start),
	JS_CFUNC_DEF("webAppletAppear", 1, nx_web_applet_appear),
	JS_CFUNC_DEF("webAppletSendMessage", 2, nx_web_applet_send_message),
	JS_CFUNC_DEF("webAppletPollMessages", 1, nx_web_applet_poll_messages),
	JS_CFUNC_DEF("webAppletRequestExit", 1, nx_web_applet_request_exit),
	JS_CFUNC_DEF("webAppletClose", 1, nx_web_applet_close),
	JS_CFUNC_DEF("webAppletIsRunning", 1, nx_web_applet_is_running),
	JS_CFUNC_DEF("webAppletGetMode", 1, nx_web_applet_get_mode),
};

void nx_init_web(JSContext *ctx, JSValueConst init_obj) {
	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_web_applet_class_id);
	JSClassDef nx_web_applet_class = {
		"WebApplet",
		.finalizer = finalizer_web_applet,
	};
	JS_NewClass(rt, nx_web_applet_class_id, &nx_web_applet_class);

	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
