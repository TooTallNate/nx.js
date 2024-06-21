#include "gamepad.h"

static JSClassID nx_gamepad_class_id;
static JSClassID nx_gamepad_button_class_id;

static u64 standard_button_masks[] = {
    HidNpadButton_B,
    HidNpadButton_A,
    HidNpadButton_Y,
    HidNpadButton_X,
    HidNpadButton_L,
    HidNpadButton_R,
    HidNpadButton_ZL,
    HidNpadButton_ZR,
    HidNpadButton_Minus,
    HidNpadButton_Plus,
    HidNpadButton_StickL,
    HidNpadButton_StickR,
    HidNpadButton_Up,
    HidNpadButton_Down,
    HidNpadButton_Left,
    HidNpadButton_Right,
};

nx_gamepad_t *nx_get_gamepad(JSContext *ctx, JSValueConst obj)
{
    return JS_GetOpaque2(ctx, obj, nx_gamepad_class_id);
}

nx_gamepad_button_t *nx_get_gamepad_button(JSContext *ctx, JSValueConst obj)
{
    return JS_GetOpaque2(ctx, obj, nx_gamepad_button_class_id);
}

static void finalizer_gamepad(JSRuntime *rt, JSValue val)
{
    nx_gamepad_t *gamepad = JS_GetOpaque(val, nx_gamepad_class_id);
    // nx_context_t *nx_ctx = JS_GetRuntimeOpaque(rt);
    if (gamepad)
    {
        // nx_ctx->pads[gamepad->id] = NULL;
        js_free_rt(rt, gamepad);
    }
}

static void finalizer_gamepad_button(JSRuntime *rt, JSValue val)
{
    nx_gamepad_button_t *gamepad_button = JS_GetOpaque(val, nx_gamepad_button_class_id);
    if (gamepad_button)
    {
        js_free_rt(rt, gamepad_button);
    }
}

static JSValue nx_gamepad_new(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    u32 id;
    if (JS_ToUint32(ctx, &id, argv[0]))
    {
        return JS_EXCEPTION;
    }

    nx_gamepad_t *gamepad = js_mallocz(ctx, sizeof(nx_gamepad_t));
    if (!gamepad)
    {
        return JS_EXCEPTION;
    }

    nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);

    gamepad->id = id;
    gamepad->pad = &nx_ctx->pads[id];

    JSValue obj = JS_NewObjectClass(ctx, nx_gamepad_class_id);
    JS_SetOpaque(obj, gamepad);

    if (id == 0)
    {
        // Index 0 is a special case, which represents input from both
        // the first controller as well as the handheld mode controller
        padInitialize(gamepad->pad, id, HidNpadIdType_Handheld);
    }
    else
    {
        padInitialize(gamepad->pad, id);
    }
    padUpdate(gamepad->pad);

    return obj;
}

static JSValue nx_gamepad_button_new(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_gamepad_t *gamepad = nx_get_gamepad(ctx, argv[0]);

    u32 index;
    if (JS_ToUint32(ctx, &index, argv[1]))
    {
        return JS_EXCEPTION;
    }

    nx_gamepad_button_t *gamepad_button = js_mallocz(ctx, sizeof(nx_gamepad_button_t));
    if (!gamepad_button)
    {
        return JS_EXCEPTION;
    }

    gamepad_button->gamepad = gamepad;
    gamepad_button->mask = standard_button_masks[index];

    JSValue obj = JS_NewObjectClass(ctx, nx_gamepad_button_class_id);
    JS_SetOpaque(obj, gamepad_button);
    return obj;
}

static JSValue nx_gamepad_get_axes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_gamepad_t *gamepad = nx_get_gamepad(ctx, this_val);
    JSValue arr = JS_NewArray(ctx);
    HidAnalogStickState analog_stick_l = padGetStickPos(gamepad->pad, 0);
    HidAnalogStickState analog_stick_r = padGetStickPos(gamepad->pad, 1);
    JS_SetPropertyUint32(ctx, arr, 0, JS_NewFloat64(ctx, (double)analog_stick_l.x / 32768.0));
    JS_SetPropertyUint32(ctx, arr, 1, JS_NewFloat64(ctx, (double)-analog_stick_l.y / 32768.0));
    JS_SetPropertyUint32(ctx, arr, 2, JS_NewFloat64(ctx, (double)analog_stick_r.x / 32768.0));
    JS_SetPropertyUint32(ctx, arr, 3, JS_NewFloat64(ctx, (double)-analog_stick_r.y / 32768.0));
    return arr;
}

static JSValue nx_gamepad_get_id(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    // nx_gamepad_t *gamepad = nx_get_gamepad(ctx, this_val);
    return JS_NewString(ctx, "");
}

static JSValue nx_gamepad_get_raw_buttons(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_gamepad_t *gamepad = nx_get_gamepad(ctx, this_val);
    return JS_NewBigUint64(ctx, padGetButtons(gamepad->pad));
}

static JSValue nx_gamepad_get_device_type(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_gamepad_t *gamepad = nx_get_gamepad(ctx, this_val);
    return JS_NewUint32(ctx, hidGetNpadDeviceType(gamepad->id));
}

static JSValue nx_gamepad_get_style_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_gamepad_t *gamepad = nx_get_gamepad(ctx, this_val);
    return JS_NewUint32(ctx, padGetStyleSet(gamepad->pad));
}

static JSValue nx_gamepad_get_index(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_gamepad_t *gamepad = nx_get_gamepad(ctx, this_val);
    return JS_NewUint32(ctx, gamepad->id);
}

static JSValue nx_gamepad_get_connected(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_gamepad_t *gamepad = nx_get_gamepad(ctx, this_val);
    return JS_NewBool(ctx, padIsConnected(gamepad->pad));
}

static JSValue nx_gamepad_button_get_pressed(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_gamepad_button_t *gamepad_button = nx_get_gamepad_button(ctx, this_val);
    u64 kDown = padGetButtons(gamepad_button->gamepad->pad);
    bool pressed = (kDown & gamepad_button->mask) != 0;
    return JS_NewBool(ctx, pressed);
}

static JSValue nx_gamepad_button_get_touched(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_gamepad_button_t *gamepad_button = nx_get_gamepad_button(ctx, this_val);
    u64 kDown = padGetButtons(gamepad_button->gamepad->pad);
    bool pressed = (kDown & gamepad_button->mask) != 0;
    return JS_NewBool(ctx, pressed);
}

static JSValue nx_gamepad_button_get_value(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_gamepad_button_t *gamepad_button = nx_get_gamepad_button(ctx, this_val);
    u64 kDown = padGetButtons(gamepad_button->gamepad->pad);
    bool pressed = (kDown & gamepad_button->mask) != 0;
    return JS_NewUint32(ctx, pressed ? 1 : 0);
}

static JSValue nx_gamepad_init_class(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSAtom atom;
    JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
    NX_DEF_GET(proto, "axes", nx_gamepad_get_axes);
    NX_DEF_GET(proto, "id", nx_gamepad_get_id);
    NX_DEF_GET(proto, "index", nx_gamepad_get_index);
    NX_DEF_GET(proto, "connected", nx_gamepad_get_connected);

    // Non-standard
    NX_DEF_GET(proto, "deviceType", nx_gamepad_get_device_type);
    NX_DEF_GET(proto, "rawButtons", nx_gamepad_get_raw_buttons);
    NX_DEF_GET(proto, "styleSet", nx_gamepad_get_style_set);

    JS_FreeValue(ctx, proto);
    return JS_UNDEFINED;
}

static JSValue nx_gamepad_button_init_class(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSAtom atom;
    JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
    NX_DEF_GET(proto, "pressed", nx_gamepad_button_get_pressed);
    NX_DEF_GET(proto, "touched", nx_gamepad_button_get_touched);
    NX_DEF_GET(proto, "value", nx_gamepad_button_get_value);
    JS_FreeValue(ctx, proto);
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
    JS_CFUNC_DEF("gamepadInit", 1, nx_gamepad_init_class),
    JS_CFUNC_DEF("gamepadNew", 1, nx_gamepad_new),
    JS_CFUNC_DEF("gamepadButtonInit", 1, nx_gamepad_button_init_class),
    JS_CFUNC_DEF("gamepadButtonNew", 1, nx_gamepad_button_new),
};

void nx_init_gamepad(JSContext *ctx, JSValueConst init_obj)
{
    JSRuntime *rt = JS_GetRuntime(ctx);

    JS_NewClassID(rt, &nx_gamepad_class_id);
    JSClassDef gamepad_class = {
        "Gamepad",
        .finalizer = finalizer_gamepad,
    };
    JS_NewClass(rt, nx_gamepad_class_id, &gamepad_class);

    JS_NewClassID(rt, &nx_gamepad_button_class_id);
    JSClassDef gamepad_button_class = {
        "GamepadButton",
        .finalizer = finalizer_gamepad_button,
    };
    JS_NewClass(rt, nx_gamepad_button_class_id, &gamepad_button_class);

    JS_SetPropertyFunctionList(ctx, init_obj, function_list, countof(function_list));
}
