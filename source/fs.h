#ifndef _NX_FS_
#define _NX_FS_

#include <switch.h>
#include <quickjs/quickjs.h>
#include "types.h"

typedef struct
{
    nx_async_result_t work;
    int err;
    const char *filename;
    uint8_t *result;
    size_t size;
} nx_fs_read_file_async_t;

JSValue js_read_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_read_file_sync(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
JSValue js_readdir_sync(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

#endif