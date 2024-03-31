#pragma once
#include "types.h"

JSValue nx_appletRequestLaunchApplication(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
void nx_init_applet(JSContext *ctx, JSValueConst init_obj);
