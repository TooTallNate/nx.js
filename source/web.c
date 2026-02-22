#include "web.h"
#include "error.h"
#include <string.h>

static JSClassID nx_web_applet_class_id;

typedef enum {
	WEB_MODE_NONE,
	WEB_MODE_WEB_SESSION,   // WebApplet with WebSession (HTTPS, window.nx)
	WEB_MODE_WIFI_AUTH,     // WifiWebAuthApplet (HTTP, localhost, no whitelist)
	WEB_MODE_OFFLINE,       // Offline applet (html-document NCA, window.nx)
} nx_web_mode_t;

typedef struct {
	// Shared state
	bool started;
	nx_web_mode_t mode;
	char *url;
	bool js_extension;
	int boot_mode;
	JSContext *ctx;

	// WebSession mode (online HTTPS)
	WebCommonConfig config;
	WebSession session;
	Event *exit_event;

	// WifiAuth mode (localhost HTTP)
	WebWifiConfig wifi_config;
	AppletHolder wifi_holder;
} nx_web_applet_t;

static nx_web_applet_t *nx_web_applet_get(JSContext *ctx, JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_web_applet_class_id);
}

static void finalizer_web_applet(JSRuntime *rt, JSValue val) {
	nx_web_applet_t *data = JS_GetOpaque(val, nx_web_applet_class_id);
	if (data) {
		if (data->started) {
			if (data->mode == WEB_MODE_WEB_SESSION || data->mode == WEB_MODE_OFFLINE) {
				WebCommonReply reply;
				webSessionWaitForExit(&data->session, &reply);
				webSessionClose(&data->session);
			} else if (data->mode == WEB_MODE_WIFI_AUTH) {
				appletHolderRequestExit(&data->wifi_holder);
				appletHolderJoin(&data->wifi_holder);
				appletHolderClose(&data->wifi_holder);
			}
		}
		if (data->url) {
			js_free_rt(rt, data->url);
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
	data->js_extension = false;
	data->boot_mode = 0;
	data->url = NULL;
	return obj;
}

static JSValue nx_web_applet_set_url(JSContext *ctx, JSValueConst this_val,
									 int argc, JSValueConst *argv) {
	nx_web_applet_t *data = nx_web_applet_get(ctx, argv[0]);
	if (!data) return JS_EXCEPTION;
	const char *url = JS_ToCString(ctx, argv[1]);
	if (!url) return JS_EXCEPTION;
	if (data->url) {
		js_free(ctx, data->url);
	}
	data->url = js_strdup(ctx, url);
	JS_FreeCString(ctx, url);
	return JS_UNDEFINED;
}

static JSValue nx_web_applet_set_js_extension(JSContext *ctx,
											  JSValueConst this_val, int argc,
											  JSValueConst *argv) {
	nx_web_applet_t *data = nx_web_applet_get(ctx, argv[0]);
	if (!data) return JS_EXCEPTION;
	data->js_extension = JS_ToBool(ctx, argv[1]);
	return JS_UNDEFINED;
}

static JSValue nx_web_applet_set_boot_mode(JSContext *ctx,
										   JSValueConst this_val, int argc,
										   JSValueConst *argv) {
	nx_web_applet_t *data = nx_web_applet_get(ctx, argv[0]);
	if (!data) return JS_EXCEPTION;
	int mode;
	if (JS_ToInt32(ctx, &mode, argv[1])) return JS_EXCEPTION;
	data->boot_mode = mode;
	return JS_UNDEFINED;
}

static bool _is_http_url(const char *url) {
	return url && strncmp(url, "http://", 7) == 0;
}

static bool _is_offline_url(const char *url) {
	return url && strncmp(url, "offline:", 8) == 0;
}

static Result _configure_web_session(nx_web_applet_t *data) {
	Result rc;

	rc = webPageCreate(&data->config, data->url);
	if (R_FAILED(rc)) return rc;

	rc = webConfigSetWhitelist(&data->config,
							  "^http://.*$\n^https://.*$");
	if (R_FAILED(rc)) return rc;

	if (data->js_extension) {
		rc = webConfigSetJsExtension(&data->config, true);
		if (R_FAILED(rc)) return rc;
	}

	webConfigSetTouchEnabledOnContents(&data->config, true);

	if (data->boot_mode != 0) {
		rc = webConfigSetBootMode(&data->config,
								 (WebSessionBootMode)data->boot_mode);
		if (R_FAILED(rc)) return rc;
	}

	return 0;
}

static Result _start_web_session(nx_web_applet_t *data) {
	Result rc = _configure_web_session(data);
	if (R_FAILED(rc)) return rc;

	webSessionCreate(&data->session, &data->config);
	rc = webSessionStart(&data->session, &data->exit_event);
	if (R_FAILED(rc)) {
		webSessionClose(&data->session);
		return rc;
	}

	data->mode = WEB_MODE_WEB_SESSION;
	return 0;
}

static Result _start_wifi_auth(nx_web_applet_t *data) {
	// WifiWebAuthApplet — no whitelist, supports localhost/LAN
	// Pass NULL for conntest_url to skip connection testing
	webWifiCreate(&data->wifi_config, NULL, data->url, (Uuid){0}, 0);

	// Launch non-blocking using the low-level applet API
	Result rc = appletCreateLibraryApplet(&data->wifi_holder,
		AppletId_LibraryAppletWifiWebAuth, LibAppletMode_AllForeground);
	if (R_FAILED(rc)) return rc;

	LibAppletArgs commonargs;
	libappletArgsCreate(&commonargs, 0);  // WifiWebAuth uses version 0
	rc = libappletArgsPush(&commonargs, &data->wifi_holder);
	if (R_FAILED(rc)) {
		appletHolderClose(&data->wifi_holder);
		return rc;
	}

	rc = libappletPushInData(&data->wifi_holder,
		&data->wifi_config.arg, sizeof(data->wifi_config.arg));
	if (R_FAILED(rc)) {
		appletHolderClose(&data->wifi_holder);
		return rc;
	}

	rc = appletHolderStart(&data->wifi_holder);
	if (R_FAILED(rc)) {
		appletHolderClose(&data->wifi_holder);
		return rc;
	}

	data->mode = WEB_MODE_WIFI_AUTH;
	return 0;
}

static Result _start_offline(nx_web_applet_t *data) {
	Result rc;

	// DocumentPath: skip "offline:" prefix (and optional '/') to get
	// a relative path. Per libnx/switchbrew docs:
	// - id=0 for OfflineHtmlPage (uses the calling application's content)
	// - Path is relative to "html-document/" in the HtmlDocument NCA RomFS
	// - Path must contain ".htdocs/"
	// - Path must not have a leading '/'
	const char *doc_path = data->url + 8;  // skip "offline:"
	if (*doc_path == '/') doc_path++;       // skip optional '/'

	rc = webOfflineCreate(&data->config, WebDocumentKind_OfflineHtmlPage,
						  0, doc_path);
	if (R_FAILED(rc)) return rc;

	if (data->js_extension) {
		rc = webConfigSetJsExtension(&data->config, true);
		if (R_FAILED(rc)) return rc;
	}

	webConfigSetTouchEnabledOnContents(&data->config, true);

	if (data->boot_mode != 0) {
		rc = webConfigSetBootMode(&data->config,
								 (WebSessionBootMode)data->boot_mode);
		if (R_FAILED(rc)) return rc;
	}

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

static JSValue nx_web_applet_start(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_web_applet_t *data = nx_web_applet_get(ctx, argv[0]);
	if (!data) return JS_EXCEPTION;

	if (data->started) {
		return JS_ThrowTypeError(ctx, "WebApplet already started");
	}

	if (!data->url) {
		return JS_ThrowTypeError(ctx, "WebApplet URL not set");
	}

	// Web applets can only be launched from Application mode
	AppletType at = appletGetAppletType();
	if (at != AppletType_Application && at != AppletType_SystemApplication) {
		return JS_ThrowTypeError(ctx,
			"WebApplet requires Application mode. "
			"Launch via NSP or hold R when opening a game to use hbmenu "
			"in Application mode.");
	}

	Result rc;
	if (_is_offline_url(data->url)) {
		// offline:/ URLs use the Offline applet (html-document NCA)
		rc = _start_offline(data);
		if (R_FAILED(rc)) {
			return nx_throw_libnx_error(ctx, rc, "Offline applet start");
		}
	} else if (_is_http_url(data->url)) {
		// HTTP URLs use WifiWebAuthApplet (no whitelist, localhost works)
		rc = _start_wifi_auth(data);
		if (R_FAILED(rc)) {
			return nx_throw_libnx_error(ctx, rc, "WifiWebAuth start");
		}
	} else {
		// HTTPS URLs use WebApplet with WebSession (window.nx messaging)
		rc = _start_web_session(data);
		if (R_FAILED(rc)) {
			return nx_throw_libnx_error(ctx, rc, "WebSession start");
		}
	}

	data->started = true;
	return JS_UNDEFINED;
}

static JSValue nx_web_applet_appear(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_web_applet_t *data = nx_web_applet_get(ctx, argv[0]);
	if (!data) return JS_EXCEPTION;
	if (!data->started || (data->mode != WEB_MODE_WEB_SESSION && data->mode != WEB_MODE_OFFLINE)) {
		return JS_ThrowTypeError(ctx,
			"appear() only available in WebSession mode (HTTPS URLs)");
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
	if (!data->started || (data->mode != WEB_MODE_WEB_SESSION && data->mode != WEB_MODE_OFFLINE)) {
		return JS_ThrowTypeError(ctx,
			"sendMessage() only available in WebSession mode (HTTPS URLs)");
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
	if (!data->started || (data->mode != WEB_MODE_WEB_SESSION && data->mode != WEB_MODE_OFFLINE)) {
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
	Result rc;
	if (data->mode == WEB_MODE_WEB_SESSION || data->mode == WEB_MODE_OFFLINE) {
		rc = webSessionRequestExit(&data->session);
	} else {
		rc = appletHolderRequestExit(&data->wifi_holder);
	}
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
		if (data->mode == WEB_MODE_WEB_SESSION || data->mode == WEB_MODE_OFFLINE) {
			WebCommonReply reply;
			webSessionWaitForExit(&data->session, &reply);
			webSessionClose(&data->session);
		} else if (data->mode == WEB_MODE_WIFI_AUTH) {
			appletHolderJoin(&data->wifi_holder);
			appletHolderClose(&data->wifi_holder);
		}
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

	if (data->mode == WEB_MODE_WEB_SESSION || data->mode == WEB_MODE_OFFLINE) {
		if (data->exit_event) {
			Result rc = eventWait(data->exit_event, 0);
			if (R_SUCCEEDED(rc)) return JS_FALSE;
		}
	} else if (data->mode == WEB_MODE_WIFI_AUTH) {
		if (appletHolderCheckFinished(&data->wifi_holder)) {
			return JS_FALSE;
		}
	}

	return JS_TRUE;
}

// Return the current mode as a string for JS inspection
static JSValue nx_web_applet_get_mode(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_web_applet_t *data = nx_web_applet_get(ctx, argv[0]);
	if (!data) return JS_EXCEPTION;
	switch (data->mode) {
		case WEB_MODE_WEB_SESSION: return JS_NewString(ctx, "web-session");
		case WEB_MODE_WIFI_AUTH: return JS_NewString(ctx, "wifi-auth");
		case WEB_MODE_OFFLINE: return JS_NewString(ctx, "offline");
		default: return JS_NewString(ctx, "none");
	}
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("webAppletNew", 0, nx_web_applet_new),
	JS_CFUNC_DEF("webAppletSetUrl", 2, nx_web_applet_set_url),
	JS_CFUNC_DEF("webAppletSetJsExtension", 2,
				  nx_web_applet_set_js_extension),
	JS_CFUNC_DEF("webAppletSetBootMode", 2, nx_web_applet_set_boot_mode),
	JS_CFUNC_DEF("webAppletStart", 1, nx_web_applet_start),
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
