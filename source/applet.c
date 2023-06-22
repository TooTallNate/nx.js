#include <switch.h>
#include "applet.h"

JSValue nx_appletGetAppletType(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    return JS_NewInt32(ctx, appletGetAppletType());
}

JSValue nx_appletGetOperationMode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    return JS_NewInt32(ctx, appletGetOperationMode());
}

static const JSCFunctionListEntry function_list[] = {
    JS_CFUNC_DEF("appletGetAppletType", 0, nx_appletGetAppletType),
    JS_CFUNC_DEF("appletGetOperationMode", 0, nx_appletGetOperationMode)};

void nx_init_applet(JSContext * ctx, JSValueConst native_obj)
{
    JS_SetPropertyFunctionList(ctx, native_obj, function_list, countof(function_list));
}
