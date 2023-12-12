#include "error.h"
#include "software-keyboard.h"

static JSClassID nx_swkbd_class_id;

typedef struct
{
	SwkbdInline kbdinline;
	SwkbdAppearArg appearArg;
	JSContext *ctx;
	JSValue instance;
	JSValue cancel_func;
	JSValue change_func;
	JSValue submit_func;
	JSValue cursor_move_func;
} nx_swkbd_t;

static nx_swkbd_t *current_kbd;

static nx_swkbd_t *nx_swkbd_get(JSContext *ctx, JSValueConst obj)
{
	return JS_GetOpaque2(ctx, obj, nx_swkbd_class_id);
}

static void finalizer_swkbd(JSRuntime *rt, JSValue val)
{
	nx_swkbd_t *data = JS_GetOpaque(val, nx_swkbd_class_id);
	if (data)
	{
		swkbdInlineClose(&data->kbdinline);
		js_free_rt(rt, data);
	}
}

void finishinit_cb(void)
{
}

void decidedcancel_cb(void)
{
	JSValue result = JS_Call(current_kbd->ctx, current_kbd->cancel_func, current_kbd->instance, 0, NULL);
	if (JS_IsException(result))
	{
		nx_emit_error_event(current_kbd->ctx);
	}
	JS_FreeValue(current_kbd->ctx, result);
	current_kbd = NULL;
}

// String changed callback.
void strchange_cb(const char *str, SwkbdChangedStringArg *arg)
{
	JSValue args[] = {
		JS_NewStringLen(current_kbd->ctx, str, arg->stringLen),
		JS_NewInt32(current_kbd->ctx, arg->cursorPos),
		JS_NewInt32(current_kbd->ctx, arg->dicStartCursorPos),
		JS_NewInt32(current_kbd->ctx, arg->dicEndCursorPos),
	};
	JSValue result = JS_Call(current_kbd->ctx, current_kbd->change_func, current_kbd->instance, countof(args), args);
	if (JS_IsException(result))
	{
		nx_emit_error_event(current_kbd->ctx);
	}
	JS_FreeValue(current_kbd->ctx, args[0]);
	JS_FreeValue(current_kbd->ctx, result);
}

// Moved cursor callback.
void movedcursor_cb(const char *str, SwkbdMovedCursorArg *arg)
{
	JSValue args[] = {
		JS_NewStringLen(current_kbd->ctx, str, arg->stringLen),
		JS_NewInt32(current_kbd->ctx, arg->cursorPos),
	};
	JSValue result = JS_Call(current_kbd->ctx, current_kbd->cursor_move_func, current_kbd->instance, countof(args), args);
	if (JS_IsException(result))
	{
		nx_emit_error_event(current_kbd->ctx);
	}
	JS_FreeValue(current_kbd->ctx, args[0]);
	JS_FreeValue(current_kbd->ctx, result);
}

// Text submitted callback, this is used to get the output text from submit.
void decidedenter_cb(const char *str, SwkbdDecidedEnterArg *arg)
{
	JSValue args[] = {
		JS_NewStringLen(current_kbd->ctx, str, arg->stringLen),
	};
	JSValue result = JS_Call(current_kbd->ctx, current_kbd->submit_func, current_kbd->instance, countof(args), args);
	if (JS_IsException(result))
	{
		nx_emit_error_event(current_kbd->ctx);
	}
	JS_FreeValue(current_kbd->ctx, args[0]);
	JS_FreeValue(current_kbd->ctx, result);
	current_kbd = NULL;
}

static JSValue nx_swkbd_create(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue obj = JS_NewObjectClass(ctx, nx_swkbd_class_id);
	nx_swkbd_t *data = js_mallocz(ctx, sizeof(nx_swkbd_t));
	if (!data)
	{
		JS_ThrowOutOfMemory(ctx);
		return JS_EXCEPTION;
	}
	JS_SetOpaque(obj, data);

	data->ctx = ctx;
	data->instance = obj;
	data->cancel_func = JS_GetPropertyStr(ctx, argv[0], "onCancel");
	data->change_func = JS_GetPropertyStr(ctx, argv[0], "onChange");
	data->submit_func = JS_GetPropertyStr(ctx, argv[0], "onSubmit");
	data->cursor_move_func = JS_GetPropertyStr(ctx, argv[0], "onCursorMove");
	JS_FreeValue(ctx, data->cancel_func);
	JS_FreeValue(ctx, data->change_func);
	JS_FreeValue(ctx, data->submit_func);
	JS_FreeValue(ctx, data->cursor_move_func);

	Result rc = swkbdInlineCreate(&data->kbdinline);
	if (R_FAILED(rc))
	{
		// TODO: throw error
	}

	swkbdInlineSetFinishedInitializeCallback(&data->kbdinline, finishinit_cb);

	// Launch the applet.
	rc = swkbdInlineLaunchForLibraryApplet(&data->kbdinline, SwkbdInlineMode_AppletDisplay, 0);
	if (R_FAILED(rc))
	{
		// TODO: throw error
	}

	// Set the callbacks.
	swkbdInlineSetChangedStringCallback(&data->kbdinline, strchange_cb);
	swkbdInlineSetMovedCursorCallback(&data->kbdinline, movedcursor_cb);
	swkbdInlineSetDecidedEnterCallback(&data->kbdinline, decidedenter_cb);
	swkbdInlineSetDecidedCancelCallback(&data->kbdinline, decidedcancel_cb);

	// Make the applet appear, can be used whenever.
	swkbdInlineMakeAppearArg(&data->appearArg, SwkbdType_Normal);

	return obj;
}

static JSValue nx_swkbd_show(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_swkbd_t *data = nx_swkbd_get(ctx, argv[0]);
	current_kbd = data;

	if (JS_ToInt32(ctx, (s32 *)&data->appearArg.type, JS_GetPropertyStr(ctx, argv[0], "type")))
		return JS_EXCEPTION;

	JSValue okButtonVal = JS_GetPropertyStr(ctx, argv[0], "okButtonText");
	if (JS_IsString(okButtonVal))
	{
		const char *okButton = JS_ToCString(ctx, okButtonVal);
		swkbdInlineAppearArgSetOkButtonText(&data->appearArg, okButton);
		JS_FreeCString(ctx, okButton);
	}
	JS_FreeValue(ctx, okButtonVal);

	JSValue leftButtonVal = JS_GetPropertyStr(ctx, argv[0], "leftButtonText");
	if (JS_IsString(leftButtonVal))
	{
		const char *leftButton = JS_ToCString(ctx, leftButtonVal);
		swkbdInlineAppearArgSetLeftButtonText(&data->appearArg, leftButton);
		JS_FreeCString(ctx, leftButton);
	}
	JS_FreeValue(ctx, leftButtonVal);

	JSValue rightButtonVal = JS_GetPropertyStr(ctx, argv[0], "rightButtonText");
	if (JS_IsString(rightButtonVal))
	{
		const char *rightButton = JS_ToCString(ctx, rightButtonVal);
		swkbdInlineAppearArgSetRightButtonText(&data->appearArg, rightButton);
		JS_FreeCString(ctx, rightButton);
	}
	JS_FreeValue(ctx, rightButtonVal);

	int dicFlag = JS_ToBool(ctx, JS_GetPropertyStr(ctx, argv[0], "enableDictionary"));
	if (dicFlag == -1)
		return JS_EXCEPTION;
	data->appearArg.dicFlag = dicFlag;

	int returnButtonFlag = JS_ToBool(ctx, JS_GetPropertyStr(ctx, argv[0], "enableReturn"));
	if (returnButtonFlag == -1)
		return JS_EXCEPTION;
	data->appearArg.returnButtonFlag = returnButtonFlag;

	if (JS_ToInt32(ctx, &data->appearArg.stringLenMin, JS_GetPropertyStr(ctx, argv[0], "minLength")))
		return JS_EXCEPTION;
	if (JS_ToInt32(ctx, &data->appearArg.stringLenMax, JS_GetPropertyStr(ctx, argv[0], "maxLength")))
		return JS_EXCEPTION;

	swkbdInlineAppear(&data->kbdinline, &data->appearArg);

	// Return the dimensions of the bounding rect
	int x = 0, y = 0, width = 0, height = 0;

	SwkbdRect keytop;
	SwkbdRect footer;
	int n = swkbdInlineGetTouchRectangles(&data->kbdinline, &keytop, &footer);
	if (n >= 1)
	{
		x += keytop.x;
		y += keytop.y;
		width += keytop.width;
		height += keytop.height;
	}
	if (n >= 2)
	{
		x += footer.x;
		y += footer.y;
		width += footer.width;
		height += footer.height;
	}

	JSValue dims = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, dims, "x", JS_NewInt32(ctx, x));
	JS_SetPropertyStr(ctx, dims, "y", JS_NewInt32(ctx, y));
	JS_SetPropertyStr(ctx, dims, "width", JS_NewInt32(ctx, width));
	JS_SetPropertyStr(ctx, dims, "height", JS_NewInt32(ctx, height));
	return dims;
}

static JSValue nx_swkbd_hide(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_swkbd_t *data = nx_swkbd_get(ctx, argv[0]);
	current_kbd = NULL;
	swkbdInlineDisappear(&data->kbdinline);
	return JS_UNDEFINED;
}

static JSValue nx_swkbd_update(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_swkbd_t *data = nx_swkbd_get(ctx, this_val);
	swkbdInlineUpdate(&data->kbdinline, NULL);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("swkbdCreate", 1, nx_swkbd_create),
	JS_CFUNC_DEF("swkbdShow", 1, nx_swkbd_show),
	JS_CFUNC_DEF("swkbdHide", 1, nx_swkbd_hide),
	JS_CFUNC_DEF("swkbdUpdate", 1, nx_swkbd_update),
};

void nx_init_swkbd(JSContext *ctx, JSValueConst init_obj)
{
	current_kbd = NULL;

	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(&nx_swkbd_class_id);
	JSClassDef nx_swkbd_class = {
		"SoftwareKeyboard",
		.finalizer = finalizer_swkbd,
	};
	JS_NewClass(rt, nx_swkbd_class_id, &nx_swkbd_class);

	JS_SetPropertyFunctionList(ctx, init_obj, function_list, countof(function_list));
}
