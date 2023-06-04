#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <pthread.h>
#include <quickjs/quickjs.h>
#include "thpool.h"
#include "fs.h"

// sleep() - TODO remove
#include <unistd.h>

void free_js_array_buffer(JSRuntime *rt, void *opaque, void *ptr)
{
    js_free_rt(rt, ptr);
}

void js_read_file_do(void *arg)
{
    nx_fs_read_file_async_t *data = (nx_fs_read_file_async_t *)arg;
    usleep(100000); // 100 ms
    data->result = "foo";
    pthread_mutex_lock(data->work.async_done_mutex);
    data->work.done = 1;
    pthread_mutex_unlock(data->work.async_done_mutex);
}

void js_read_file_cb(void *arg)
{
    printf("cb\n");
    nx_fs_read_file_async_t *data = (nx_fs_read_file_async_t *)arg;
    JS_FreeCString(data->work.ctx, data->filename);

    JSValue result = JS_NewString(data->work.ctx, data->result);
    JSValue args[] = {result};
    JSValue ret_val = JS_Call(data->work.ctx, data->work.js_callback, JS_NULL, 1, args);
    JS_FreeValue(data->work.ctx, data->work.js_callback);
    if (JS_IsException(ret_val))
    {
        //p(data->work.ctx);
    }
    JS_FreeValue(data->work.ctx, ret_val);
    free(data);
}

JSValue js_read_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
    nx_fs_read_file_async_t *data = malloc(sizeof(nx_fs_read_file_async_t));
    memset(data, 0, sizeof(nx_fs_read_file_async_t));
    data->filename = JS_ToCString(ctx, argv[0]);
    data->work.ctx = ctx;
    data->work.callback = js_read_file_cb;
    data->work.async_done_mutex = &nx_ctx->async_done_mutex;
    data->work.js_callback = JS_DupValue(ctx, argv[1]);
    nx_ctx->work = data;
    thpool_add_work(nx_ctx->thpool, js_read_file_do, data);
    return JS_UNDEFINED;
}

JSValue js_read_file_sync(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *filename = JS_ToCString(ctx, argv[0]);
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
    {
        JS_ThrowTypeError(ctx, "File not found: %s", filename);
        JS_FreeCString(ctx, filename);
        return JS_EXCEPTION;
    }

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    rewind(file);

    uint8_t *buffer = js_malloc(ctx, size);
    if (buffer == NULL)
    {
        JS_FreeCString(ctx, filename);
        fclose(file);
        JS_ThrowOutOfMemory(ctx);
        return JS_EXCEPTION;
    }

    size_t result = fread(buffer, 1, size, file);
    fclose(file);

    if (result != size)
    {
        JS_FreeCString(ctx, filename);
        js_free(ctx, buffer);
        JS_ThrowTypeError(ctx, "Failed to read entire file. Got %lu, expected %lu", result, result);
        return JS_EXCEPTION;
    }

    return JS_NewArrayBuffer(ctx, buffer, size, free_js_array_buffer, NULL, false);
}
