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
	char *url;
	bool js_extension;
	int boot_mode;
	JSContext *ctx;

	// Display options
	int boot_display_kind;       // -1 = not set
	int background_kind;         // -1 = not set
	int footer;                  // -1 = not set, 0/1
	int pointer;                 // -1 = not set, 0/1
	int left_stick_mode;         // -1 = not set
	int boot_as_media_player;    // -1 = not set, 0/1
	int screen_shot;             // -1 = not set, 0/1 (Web only)
	int page_cache;              // -1 = not set, 0/1
	int web_audio;               // -1 = not set, 0/1
	int footer_fixed_kind;       // -1 = not set
	int page_fade;               // -1 = not set, 0/1
	int boot_loading_icon;       // -1 = not set, 0/1 (Offline only)
	int page_scroll_indicator;   // -1 = not set, 0/1
	int media_player_speed_control; // -1 = not set, 0/1
	int media_auto_play;         // -1 = not set, 0/1
	float override_web_audio_volume;   // -1 = not set
	float override_media_audio_volume; // -1 = not set
	int media_player_auto_close; // -1 = not set, 0/1
	int media_player_ui;         // -1 = not set, 0/1 (Offline only)
	char *user_agent_additional; // NULL = not set (Web only)

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
		if (data->url) {
			js_free_rt(rt, data->url);
		}
		if (data->user_agent_additional) {
			js_free_rt(rt, data->user_agent_additional);
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
	data->boot_display_kind = -1;
	data->background_kind = -1;
	data->footer = -1;
	data->pointer = -1;
	data->left_stick_mode = -1;
	data->boot_as_media_player = -1;
	data->screen_shot = -1;
	data->page_cache = -1;
	data->web_audio = -1;
	data->footer_fixed_kind = -1;
	data->page_fade = -1;
	data->boot_loading_icon = -1;
	data->page_scroll_indicator = -1;
	data->media_player_speed_control = -1;
	data->media_auto_play = -1;
	data->override_web_audio_volume = -1.0f;
	data->override_media_audio_volume = -1.0f;
	data->media_player_auto_close = -1;
	data->media_player_ui = -1;
	data->user_agent_additional = NULL;
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

#define SETTER_BOOL(name, field) \
static JSValue nx_web_applet_set_##name(JSContext *ctx, \
	JSValueConst this_val, int argc, JSValueConst *argv) { \
	nx_web_applet_t *data = nx_web_applet_get(ctx, argv[0]); \
	if (!data) return JS_EXCEPTION; \
	data->field = JS_ToBool(ctx, argv[1]); \
	return JS_UNDEFINED; \
}

#define SETTER_INT(name, field) \
static JSValue nx_web_applet_set_##name(JSContext *ctx, \
	JSValueConst this_val, int argc, JSValueConst *argv) { \
	nx_web_applet_t *data = nx_web_applet_get(ctx, argv[0]); \
	if (!data) return JS_EXCEPTION; \
	int val; \
	if (JS_ToInt32(ctx, &val, argv[1])) return JS_EXCEPTION; \
	data->field = val; \
	return JS_UNDEFINED; \
}

#define SETTER_FLOAT(name, field) \
static JSValue nx_web_applet_set_##name(JSContext *ctx, \
	JSValueConst this_val, int argc, JSValueConst *argv) { \
	nx_web_applet_t *data = nx_web_applet_get(ctx, argv[0]); \
	if (!data) return JS_EXCEPTION; \
	double val; \
	if (JS_ToFloat64(ctx, &val, argv[1])) return JS_EXCEPTION; \
	data->field = (float)val; \
	return JS_UNDEFINED; \
}

SETTER_INT(boot_display_kind, boot_display_kind)
SETTER_INT(background_kind, background_kind)
SETTER_BOOL(footer, footer)
SETTER_BOOL(pointer, pointer)
SETTER_INT(left_stick_mode, left_stick_mode)
SETTER_BOOL(boot_as_media_player, boot_as_media_player)
SETTER_BOOL(screen_shot, screen_shot)
SETTER_BOOL(page_cache, page_cache)
SETTER_BOOL(web_audio, web_audio)
SETTER_INT(footer_fixed_kind, footer_fixed_kind)
SETTER_BOOL(page_fade, page_fade)
SETTER_BOOL(boot_loading_icon, boot_loading_icon)
SETTER_BOOL(page_scroll_indicator, page_scroll_indicator)
SETTER_BOOL(media_player_speed_control, media_player_speed_control)
SETTER_BOOL(media_auto_play, media_auto_play)
SETTER_FLOAT(override_web_audio_volume, override_web_audio_volume)
SETTER_FLOAT(override_media_audio_volume, override_media_audio_volume)
SETTER_BOOL(media_player_auto_close, media_player_auto_close)
SETTER_BOOL(media_player_ui, media_player_ui)

static JSValue nx_web_applet_set_user_agent_additional(JSContext *ctx,
	JSValueConst this_val, int argc, JSValueConst *argv) {
	nx_web_applet_t *data = nx_web_applet_get(ctx, argv[0]);
	if (!data) return JS_EXCEPTION;
	const char *str = JS_ToCString(ctx, argv[1]);
	if (!str) return JS_EXCEPTION;
	if (data->user_agent_additional) {
		js_free(ctx, data->user_agent_additional);
	}
	data->user_agent_additional = js_strdup(ctx, str);
	JS_FreeCString(ctx, str);
	return JS_UNDEFINED;
}

static void _apply_common_config(nx_web_applet_t *data) {
	if (data->boot_display_kind >= 0)
		webConfigSetBootDisplayKind(&data->config, (u32)data->boot_display_kind);
	if (data->background_kind >= 0)
		webConfigSetBackgroundKind(&data->config, (WebBackgroundKind)data->background_kind);
	if (data->footer >= 0)
		webConfigSetFooter(&data->config, (bool)data->footer);
	if (data->pointer >= 0)
		webConfigSetPointer(&data->config, (bool)data->pointer);
	if (data->left_stick_mode >= 0)
		webConfigSetLeftStickMode(&data->config, (WebLeftStickMode)data->left_stick_mode);
	if (data->boot_as_media_player >= 0)
		webConfigSetBootAsMediaPlayer(&data->config, (bool)data->boot_as_media_player);
	if (data->page_cache >= 0)
		webConfigSetPageCache(&data->config, (bool)data->page_cache);
	if (data->web_audio >= 0)
		webConfigSetWebAudio(&data->config, (bool)data->web_audio);
	if (data->footer_fixed_kind >= 0)
		webConfigSetFooterFixedKind(&data->config, (WebFooterFixedKind)data->footer_fixed_kind);
	if (data->page_fade >= 0)
		webConfigSetPageFade(&data->config, (bool)data->page_fade);
	if (data->page_scroll_indicator >= 0)
		webConfigSetPageScrollIndicator(&data->config, (bool)data->page_scroll_indicator);
	if (data->media_player_speed_control >= 0)
		webConfigSetMediaPlayerSpeedControl(&data->config, (bool)data->media_player_speed_control);
	if (data->media_auto_play >= 0)
		webConfigSetMediaAutoPlay(&data->config, (bool)data->media_auto_play);
	if (data->override_web_audio_volume >= 0.0f)
		webConfigSetOverrideWebAudioVolume(&data->config, data->override_web_audio_volume);
	if (data->override_media_audio_volume >= 0.0f)
		webConfigSetOverrideMediaAudioVolume(&data->config, data->override_media_audio_volume);
	if (data->media_player_auto_close >= 0)
		webConfigSetMediaPlayerAutoClose(&data->config, (bool)data->media_player_auto_close);
}

static bool _is_offline_url(const char *url) {
	return url && strncmp(url, "offline:", 8) == 0;
}

static Result _start_web_session(nx_web_applet_t *data) {
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

	_apply_common_config(data);

	// Web-only options
	if (data->screen_shot >= 0)
		webConfigSetScreenShot(&data->config, (bool)data->screen_shot);
	if (data->user_agent_additional)
		webConfigSetUserAgentAdditionalString(&data->config, data->user_agent_additional);

	webSessionCreate(&data->session, &data->config);
	rc = webSessionStart(&data->session, &data->exit_event);
	if (R_FAILED(rc)) {
		webSessionClose(&data->session);
		return rc;
	}

	data->mode = WEB_MODE_WEB_SESSION;
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

	_apply_common_config(data);

	// Offline-only options
	if (data->boot_loading_icon >= 0)
		webConfigSetBootLoadingIcon(&data->config, (bool)data->boot_loading_icon);
	if (data->media_player_ui >= 0)
		webConfigSetMediaPlayerUi(&data->config, (bool)data->media_player_ui);

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
		// offline: URLs use the Offline applet (html-document NCA)
		rc = _start_offline(data);
		if (R_FAILED(rc)) {
			return nx_throw_libnx_error(ctx, rc, "Offline applet start");
		}
	} else {
		// HTTP/HTTPS URLs use WebApplet with WebSession (window.nx messaging)
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

// Return the current mode as a string for JS inspection
static JSValue nx_web_applet_get_mode(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_web_applet_t *data = nx_web_applet_get(ctx, argv[0]);
	if (!data) return JS_EXCEPTION;
	switch (data->mode) {
		case WEB_MODE_WEB_SESSION: return JS_NewString(ctx, "web-session");
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
	JS_CFUNC_DEF("webAppletSetBootDisplayKind", 2, nx_web_applet_set_boot_display_kind),
	JS_CFUNC_DEF("webAppletSetBackgroundKind", 2, nx_web_applet_set_background_kind),
	JS_CFUNC_DEF("webAppletSetFooter", 2, nx_web_applet_set_footer),
	JS_CFUNC_DEF("webAppletSetPointer", 2, nx_web_applet_set_pointer),
	JS_CFUNC_DEF("webAppletSetLeftStickMode", 2, nx_web_applet_set_left_stick_mode),
	JS_CFUNC_DEF("webAppletSetBootAsMediaPlayer", 2, nx_web_applet_set_boot_as_media_player),
	JS_CFUNC_DEF("webAppletSetScreenShot", 2, nx_web_applet_set_screen_shot),
	JS_CFUNC_DEF("webAppletSetPageCache", 2, nx_web_applet_set_page_cache),
	JS_CFUNC_DEF("webAppletSetWebAudio", 2, nx_web_applet_set_web_audio),
	JS_CFUNC_DEF("webAppletSetFooterFixedKind", 2, nx_web_applet_set_footer_fixed_kind),
	JS_CFUNC_DEF("webAppletSetPageFade", 2, nx_web_applet_set_page_fade),
	JS_CFUNC_DEF("webAppletSetBootLoadingIcon", 2, nx_web_applet_set_boot_loading_icon),
	JS_CFUNC_DEF("webAppletSetPageScrollIndicator", 2, nx_web_applet_set_page_scroll_indicator),
	JS_CFUNC_DEF("webAppletSetMediaPlayerSpeedControl", 2, nx_web_applet_set_media_player_speed_control),
	JS_CFUNC_DEF("webAppletSetMediaAutoPlay", 2, nx_web_applet_set_media_auto_play),
	JS_CFUNC_DEF("webAppletSetOverrideWebAudioVolume", 2, nx_web_applet_set_override_web_audio_volume),
	JS_CFUNC_DEF("webAppletSetOverrideMediaAudioVolume", 2, nx_web_applet_set_override_media_audio_volume),
	JS_CFUNC_DEF("webAppletSetMediaPlayerAutoClose", 2, nx_web_applet_set_media_player_auto_close),
	JS_CFUNC_DEF("webAppletSetMediaPlayerUi", 2, nx_web_applet_set_media_player_ui),
	JS_CFUNC_DEF("webAppletSetUserAgentAdditionalString", 2, nx_web_applet_set_user_agent_additional),
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
