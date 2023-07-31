#include <png.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <cairo.h>
#include <quickjs/quickjs.h>
#include "image.h"
#include "async.h"

static JSClassID nx_image_class_id;

typedef struct
{
    int err;
    uint8_t *input;
    size_t input_size;
    nx_image_t *output;
    int width;
    int height;
} nx_decode_image_async_t;

struct buffer_state
{
    uint8_t *ptr;
    size_t size;
};

nx_image_t *nx_get_image(JSContext *ctx, JSValueConst obj)
{
    return JS_GetOpaque2(ctx, obj, nx_image_class_id);
}

void free_image(nx_image_t *image)
{
    if (image)
    {
        cairo_surface_destroy(image->surface);
        free(image->buffer);
        free(image);
    }
}

void user_read_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
    struct buffer_state *state = (struct buffer_state *)png_get_io_ptr(png_ptr);
    memcpy(data, state->ptr, length);
    state->ptr += length;
}

uint8_t *decode_png(uint8_t *input, size_t input_size, int *width, int *height)
{
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);

    struct buffer_state state = {input, input_size};
    png_set_read_fn(png_ptr, &state, user_read_data);

    png_read_info(png_ptr, info_ptr);

    *width = png_get_image_width(png_ptr, info_ptr);
    *height = png_get_image_height(png_ptr, info_ptr);

    png_set_bgr(png_ptr);
    png_set_expand(png_ptr);
    png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);

    uint8_t *image_data = malloc(4 * (*width) * (*height));
    png_bytep rows[*height];
    for (int i = 0; i < *height; ++i)
    {
        rows[i] = image_data + i * 4 * (*width);
    }

    png_read_image(png_ptr, rows);

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    return image_data;
}

void nx_decode_image_do(nx_work_t *req)
{
    nx_decode_image_async_t *data = (nx_decode_image_async_t *)req->data;
    nx_image_t *image = malloc(sizeof(nx_image_t));
    image->buffer = decode_png(data->input, data->input_size, &data->width, &data->height);
    image->surface = cairo_image_surface_create_for_data(
        image->buffer, CAIRO_FORMAT_ARGB32, data->width, data->height, data->width * 4);
    data->output = image;
}

void nx_decode_image_cb(JSContext *ctx, nx_work_t *req, JSValue *args)
{
    nx_decode_image_async_t *data = (nx_decode_image_async_t *)req->data;

    if (data->err)
    {
        args[0] = JS_NewError(ctx);
        JS_DefinePropertyValueStr(ctx, args[0], "message", JS_NewString(ctx, strerror(data->err)), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
        return;
    }

    nx_image_t *image = data->output;
    JSValue obj = JS_NewObject(ctx);
    JSValue opaque = JS_NewObjectClass(ctx, nx_image_class_id);
    if (JS_IsException(opaque))
    {
        free_image(image);
        args[0] = opaque;
        return;
    }

    JS_SetOpaque(opaque, image);
    JS_SetPropertyStr(ctx, obj, "opaque", opaque);
    JS_SetPropertyStr(ctx, obj, "width", JS_NewInt32(ctx, data->width));
    JS_SetPropertyStr(ctx, obj, "height", JS_NewInt32(ctx, data->height));
    args[1] = obj;
}

JSValue nx_decode_image(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    size_t buffer_size;
    JSValue buffer_val = JS_DupValue(ctx, argv[1]);
    NX_INIT_WORK_T(nx_decode_image_async_t);
    data->input = JS_GetArrayBuffer(ctx, &buffer_size, buffer_val);
    data->input_size = buffer_size;

    nx_queue_async(ctx, req, nx_decode_image_do, nx_decode_image_cb, argv[0]);
    return JS_UNDEFINED;
}

static void finalizer_image(JSRuntime *rt, JSValue val)
{
    nx_image_t *image = JS_GetOpaque(val, nx_image_class_id);
    free_image(image);
}

static const JSCFunctionListEntry function_list[] = {
    JS_CFUNC_DEF("decodeImage", 0, nx_decode_image),
};

void nx_init_image(JSContext *ctx, JSValueConst native_obj)
{
    JSRuntime *rt = JS_GetRuntime(ctx);

    JS_NewClassID(&nx_image_class_id);
    JSClassDef font_face_class = {
        "nx_image_t",
        .finalizer = finalizer_image,
    };
    JS_NewClass(rt, nx_image_class_id, &font_face_class);

    JS_SetPropertyFunctionList(ctx, native_obj, function_list, countof(function_list));
}
