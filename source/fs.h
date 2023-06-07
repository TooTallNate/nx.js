#pragma once
#include <quickjs/quickjs.h>

JSValue js_read_file_sync(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_readdir_sync(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
