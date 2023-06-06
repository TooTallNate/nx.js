#include <stdbool.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <errno.h>
#include <quickjs/quickjs.h>
#include "fs.h"
#include "async.h"

JSValue js_readdir_sync(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    DIR *dir;
    struct dirent *entry;

    const char *path = JS_ToCString(ctx, argv[0]);
    dir = opendir(path);
    if (dir == NULL)
    {
        JSValue error = JS_NewString(ctx, "An error occurred");
        JS_Throw(ctx, error);
        return JS_UNDEFINED;
    }

    int i = 0;
    JSValue arr = JS_NewArray(ctx);
    while ((entry = readdir(dir)) != NULL)
    {
        // Filter out `.` and `..`
        if (!strcmp(".", entry->d_name) || !strcmp("..", entry->d_name))
        {
            continue;
        }
        JS_SetPropertyUint32(ctx, arr, i, JS_NewString(ctx, entry->d_name));
        i++;
    }

    closedir(dir);

    return arr;
}

void free_array_buffer(JSRuntime *rt, void *opaque, void *ptr)
{
    free(ptr);
}

void free_js_array_buffer(JSRuntime *rt, void *opaque, void *ptr)
{
    js_free_rt(rt, ptr);
}

void js_read_file_do(nx_work_t *req)
{
    nx_fs_read_file_async_t *data = (nx_fs_read_file_async_t *)req->data;
    FILE *file = fopen(data->filename, "rb");
    if (file == NULL)
    {
        data->err = errno;
        return;
    }

    fseek(file, 0, SEEK_END);
    data->size = ftell(file);
    rewind(file);

    data->result = malloc(data->size);
    if (data->result == NULL)
    {
        fclose(file);
        return;
    }

    size_t result = fread(data->result, 1, data->size, file);
    fclose(file);

    if (result != data->size)
    {
        free(data->result);
        data->result = NULL;
        data->err = -1;
    }
}

void js_read_file_cb(JSContext *ctx, nx_work_t *req, JSValue *args)
{
    nx_fs_read_file_async_t *data = (nx_fs_read_file_async_t *)req->data;
    JS_FreeCString(ctx, data->filename);
    args[1] = JS_NewArrayBuffer(ctx, data->result, data->size, free_array_buffer, NULL, false);
}

JSValue js_read_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    NX_INIT_WORK_T(nx_fs_read_file_async_t);
    data->filename = JS_ToCString(ctx, argv[1]);
    nx_queue_async(ctx, req, js_read_file_do, js_read_file_cb, argv[0]);
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
