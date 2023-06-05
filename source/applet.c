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
