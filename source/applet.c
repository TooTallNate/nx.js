#include "applet.h"

static JSValue nx_applet_illuminance(JSContext *ctx, JSValueConst this_val,
									 int argc, JSValueConst *argv) {
	float illuminance = 0.0f;
	Result rc = appletGetCurrentIlluminance(&illuminance);
	if (R_FAILED(rc)) {
		JS_ThrowInternalError(
			ctx, "appletGetCurrentIlluminance() returned 0x%x", rc);
		return JS_EXCEPTION;
	}
	return JS_NewFloat64(ctx, illuminance);
}

JSValue nx_appletGetAppletType(JSContext *ctx, JSValueConst this_val, int argc,
							   JSValueConst *argv) {
	return JS_NewInt32(ctx, appletGetAppletType());
}

JSValue nx_appletGetOperationMode(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	return JS_NewInt32(ctx, appletGetOperationMode());
}

JSValue nx_appletSetMediaPlaybackState(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	int state = JS_ToBool(ctx, argv[0]);
	if (state < 0) {
		return JS_EXCEPTION;
	}
	Result rc = appletSetMediaPlaybackState(state);
	if (R_FAILED(rc)) {
		JS_ThrowInternalError(
			ctx, "appletSetMediaPlaybackState() returned 0x%x", rc);
		return JS_EXCEPTION;
	}
	return JS_UNDEFINED;
}

JSValue nx_appletRequestLaunchApplication(JSContext *ctx, JSValueConst this_val,
										  int argc, JSValueConst *argv) {
	u64 app_id;
	if (JS_ToBigUint64(ctx, &app_id, JS_GetPropertyStr(ctx, this_val, "id"))) {
		return JS_EXCEPTION;
	}
	appletRequestLaunchApplication(app_id, NULL);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("appletIlluminance", 0, nx_applet_illuminance),
	JS_CFUNC_DEF("appletGetAppletType", 0, nx_appletGetAppletType),
	JS_CFUNC_DEF("appletGetOperationMode", 0, nx_appletGetOperationMode),
	JS_CFUNC_DEF("appletSetMediaPlaybackState", 1,
				 nx_appletSetMediaPlaybackState),
};

void nx_init_applet(JSContext *ctx, JSValueConst init_obj) {
	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
