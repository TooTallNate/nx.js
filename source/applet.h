#ifndef _NX_APPLET_
#define _NX_APPLET_

#include <quickjs/quickjs.h>

JSValue js_appletGetAppletType(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_appletGetOperationMode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

#endif
