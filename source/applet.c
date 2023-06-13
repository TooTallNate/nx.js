#include <switch.h>
#include "applet.h"

JSValue js_appletGetAppletType(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    return JS_NewInt32(ctx, appletGetAppletType());
}

JSValue js_appletGetOperationMode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    return JS_NewInt32(ctx, appletGetOperationMode());
}

static const JSCFunctionListEntry function_list[] = {
    JS_CFUNC_DEF("appletGetAppletType", 0, js_appletGetAppletType),
    JS_CFUNC_DEF("appletGetOperationMode", 0, js_appletGetOperationMode)};

void nx_init_applet(JSContext * ctx, JSValueConst native_obj)
{
    JS_SetPropertyFunctionList(ctx, native_obj, function_list, sizeof(function_list) / sizeof(function_list[0]));
}
