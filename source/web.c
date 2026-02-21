#include "web.h"
#include "error.h"
#include <string.h>

static JSClassID nx_web_applet_class_id;

typedef struct {
	WebCommonConfig config;
	WebSession session;
	Event *exit_event;
	bool started;
	
	bool js_extension;
	int boot_mode;
	char *url;
	JSContext *ctx;
} nx_web_applet_t;

static nx_web_applet_t *nx_web_applet_get(JSContext *ctx, JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_web_applet_class_id);
}

static void finalizer_web_applet(JSRuntime *rt, JSValue val) {
	nx_web_applet_t *data = JS_GetOpaque(val, nx_web_applet_class_id);
	if (data) {
		if (data->started ) {
			WebCommonReply reply;
			webSessionWaitForExit(&data->session, &reply);
			webSessionClose(&data->session);
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

static Result _web_applet_configure(nx_web_applet_t *data) {
	Result rc;

	rc = webPageCreate(&data->config, data->url);
	if (R_FAILED(rc)) return rc;

	// Set whitelist to allow all URLs
	rc = webConfigSetWhitelist(&data->config,
							  "^http://.*$\n^https://.*$");
	if (R_FAILED(rc)) return rc;

	if (data->js_extension) {
		rc = webConfigSetJsExtension(&data->config, true);
		if (R_FAILED(rc)) return rc;
	}

	// Enable touch
	webConfigSetTouchEnabledOnContents(&data->config, true);

	if (data->boot_mode != 0) {
		rc = webConfigSetBootMode(&data->config,
								 (WebSessionBootMode)data->boot_mode);
		if (R_FAILED(rc)) return rc;
	}

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
		return JS_ThrowTypeError(ctx, "WebApplet URL or document path not set");
	}

	// Web applets can only be launched from Application mode
	AppletType at = appletGetAppletType();
	if (at != AppletType_Application && at != AppletType_SystemApplication) {
		return JS_ThrowTypeError(ctx,
			"WebApplet requires Application mode. "
			"Launch via NSP or hold R when opening a game to use hbmenu "
			"in Application mode.");
	}

	Result rc = _web_applet_configure(data);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "WebApplet configure");
	}

	// Use WebSession for async mode
	webSessionCreate(&data->session, &data->config);

	rc = webSessionStart(&data->session, &data->exit_event);
	if (R_FAILED(rc)) {
		webSessionClose(&data->session);
		return nx_throw_libnx_error(ctx, rc, "webSessionStart()");
	}

	data->started = true;
	
	return JS_UNDEFINED;
}

static JSValue nx_web_applet_appear(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_web_applet_t *data = nx_web_applet_get(ctx, argv[0]);
	if (!data) return JS_EXCEPTION;
	if (!data->started || false) {
		return JS_ThrowTypeError(ctx, "WebApplet not started in session mode");
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
	if (!data->started || false) {
		return JS_ThrowTypeError(ctx, "WebApplet not started in session mode");
	}
	size_t len;
	const char *msg = JS_ToCStringLen(ctx, &len, argv[1]);
	if (!msg) return JS_EXCEPTION;
	bool flag = false;
	Result rc = webSessionTrySendContentMessage(&data->session, msg, (u32)len,
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
	if (!data->started || false) {
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
	if (!data->started || false) {
		return JS_ThrowTypeError(ctx, "WebApplet not started in session mode");
	}
	Result rc = webSessionRequestExit(&data->session);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "webSessionRequestExit()");
	}
	return JS_UNDEFINED;
}

static JSValue nx_web_applet_close(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_web_applet_t *data = nx_web_applet_get(ctx, argv[0]);
	if (!data) return JS_EXCEPTION;
	if (data->started ) {
		WebCommonReply reply;
		webSessionWaitForExit(&data->session, &reply);
		webSessionClose(&data->session);
		data->started = false;
		
	}
	return JS_UNDEFINED;
}

static JSValue nx_web_applet_is_running(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	nx_web_applet_t *data = nx_web_applet_get(ctx, argv[0]);
	if (!data) return JS_EXCEPTION;
	if (!data->started || false) {
		return JS_FALSE;
	}
	bool running = true;
	if (data->exit_event) {
		Result rc = eventWait(data->exit_event, 0);
		if (R_SUCCEEDED(rc)) {
			running = false;
		}
	}
	return JS_NewBool(ctx, running);
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
