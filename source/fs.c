#include <stdbool.h>
#include <dirent.h>
#include <string.h>
#include "fs.h"

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

void free_js_array_buffer(JSRuntime *rt, void *opaque, void *ptr)
{
    js_free_rt(rt, ptr);
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

static const JSCFunctionListEntry function_list[] = {
    JS_CFUNC_DEF("readDirSync", 0, js_readdir_sync),
    JS_CFUNC_DEF("readFileSync", 0, js_read_file_sync)};

void nx_init_fs(JSContext *ctx, JSValueConst native_obj)
{
    JS_SetPropertyFunctionList(ctx, native_obj, function_list, sizeof(function_list) / sizeof(function_list[0]));
}
