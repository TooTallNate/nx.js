#include "font.h"
#include "canvas.h"

#define RECT_ARGS                                        \
    double args[4];                                      \
    if (js_validate_doubles_args(ctx, argv, args, 4, 1)) \
    {                                                    \
        JS_ThrowTypeError(ctx, "invalid input");         \
        return JS_EXCEPTION;                             \
    }                                                    \
    double x = args[0];                                  \
    double y = args[1];                                  \
    double width = args[2];                              \
    double height = args[3];

static JSClassID nx_canvas_context_class_id;

static int js_validate_doubles_args(JSContext *ctx, JSValueConst *argv, double *args, size_t count, size_t offset)
{
    int result = 0;
    for (size_t i = 0; i < count; i++)
    {
        if (JS_ToFloat64(ctx, &args[i], argv[offset + i]))
        {
            result = 1;
            break;
        }
    }
    return result;
}

static JSValue js_canvas_new_context(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int width;
    int height;
    if (JS_ToInt32(ctx, &width, argv[0]) ||
        JS_ToInt32(ctx, &height, argv[1]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    size_t buf_size = width * height * 4;
    uint8_t *buffer = js_malloc(ctx, buf_size);
    if (!buffer)
    {
        JS_ThrowOutOfMemory(ctx);
        return JS_EXCEPTION;
    }
    memset(buffer, 0, buf_size);

    nx_canvas_context_2d_t *context = js_malloc(ctx, sizeof(nx_canvas_context_2d_t));
    if (!context)
    {
        JS_ThrowOutOfMemory(ctx);
        return JS_EXCEPTION;
    }
    memset(context, 0, sizeof(nx_canvas_context_2d_t));

    JSValue obj = JS_NewObjectClass(ctx, nx_canvas_context_class_id);
    if (JS_IsException(obj))
    {
        free(context);
        return obj;
    }

    // TOOD: this probably needs to go into `framebuffer_init` instead
    JS_DupValue(ctx, obj);

    // On Switch, the byte order seems to be BGRA
    cairo_surface_t *surface = cairo_image_surface_create_for_data(
        buffer, CAIRO_FORMAT_ARGB32, width, height, width * 4);

    context->width = width;
    context->height = height;
    context->data = buffer;
    context->surface = surface;
    context->ctx = cairo_create(surface);

    cairo_set_font_size(context->ctx, 46);

    JS_SetOpaque(obj, context);
    return obj;
}

static JSValue js_canvas_begin_path(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    cairo_new_path(context->ctx);
    return JS_UNDEFINED;
}

static JSValue js_canvas_close_path(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    cairo_close_path(context->ctx);
    return JS_UNDEFINED;
}

static JSValue js_canvas_fill(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    cairo_fill(context->ctx);
    return JS_UNDEFINED;
}

static JSValue js_canvas_stroke(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    cairo_stroke(context->ctx);
    return JS_UNDEFINED;
}

static JSValue js_canvas_move_to(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    double args[2];
    if (js_validate_doubles_args(ctx, argv, args, 2, 1))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    cairo_move_to(context->ctx, args[0], args[1]);
    return JS_UNDEFINED;
}

static JSValue js_canvas_line_to(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    double args[2];
    if (js_validate_doubles_args(ctx, argv, args, 2, 1))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    cairo_line_to(context->ctx, args[0], args[1]);
    return JS_UNDEFINED;
}

static JSValue js_canvas_rect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    RECT_ARGS;
    if (width == 0)
    {
        cairo_move_to(context->ctx, x, y);
        cairo_line_to(context->ctx, x, y + height);
    }
    else if (height == 0)
    {
        cairo_move_to(context->ctx, x, y);
        cairo_line_to(context->ctx, x + width, y);
    }
    else
    {
        cairo_rectangle(context->ctx, x, y, width, height);
    }
    return JS_UNDEFINED;
}

static JSValue js_canvas_set_source_rgba(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    double args[4];
    if (js_validate_doubles_args(ctx, argv, args, 4, 1))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    cairo_set_source_rgba(context->ctx, args[0] / 255., args[1] / 255., args[2] / 255., args[3]);
    return JS_UNDEFINED;
}

static JSValue js_canvas_set_font(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    nx_font_face_t *face = nx_get_font_face(ctx, argv[1]);
    if (!face)
    {
        return JS_EXCEPTION;
    }
    double font_size;
    if (JS_ToFloat64(ctx, &font_size, argv[2]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    cairo_set_font_face(context->ctx, face->cairo_font);
    cairo_set_font_size(context->ctx, font_size);
    context->ft_face = face->ft_face;
    return JS_UNDEFINED;
}

static JSValue js_canvas_get_line_width(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    return JS_NewFloat64(ctx, cairo_get_line_width(context->ctx));
}

static JSValue js_canvas_set_line_width(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    double n;
    if (JS_ToFloat64(ctx, &n, argv[1]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    cairo_set_line_width(context->ctx, n);
    return JS_UNDEFINED;
}

static JSValue js_canvas_get_line_dash(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    int count = cairo_get_dash_count(context->ctx);
    double dashes[count];
    cairo_get_dash(context->ctx, dashes, NULL);

    JSValue array = JS_NewArray(ctx);
    for (int i = 0; i < count; i++)
    {
        JS_SetPropertyUint32(ctx, array, i, JS_NewFloat64(ctx, dashes[i]));
    }
    return array;
}

static JSValue js_canvas_set_line_dash(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    JSValue length_val = JS_GetPropertyStr(ctx, argv[1], "length");
    uint32_t length;
    if (JS_ToUint32(ctx, &length, length_val))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    uint32_t num_dashes = length & 1 ? length * 2 : length;
    uint32_t zero_dashes = 0;
    double dashes[num_dashes];
    for (uint32_t i = 0; i < num_dashes; i++)
    {
        if (JS_ToFloat64(ctx, &dashes[i], JS_GetPropertyUint32(ctx, argv[1], i % length)))
        {
            return JS_UNDEFINED;
        }
        if (dashes[i] == 0)
            zero_dashes++;
    }
    double offset;
    cairo_get_dash(context->ctx, NULL, &offset);
    if (zero_dashes == num_dashes)
    {
        cairo_set_dash(context->ctx, NULL, 0, offset);
    }
    else
    {
        cairo_set_dash(context->ctx, dashes, num_dashes, offset);
    }
    return JS_UNDEFINED;
}

static JSValue js_canvas_rotate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    double n;
    if (JS_ToFloat64(ctx, &n, argv[1]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    cairo_rotate(context->ctx, n);
    return JS_UNDEFINED;
}

static JSValue js_canvas_translate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    double args[2];
    if (js_validate_doubles_args(ctx, argv, args, 2, 1))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    cairo_translate(context->ctx, args[0], args[1]);
    return JS_UNDEFINED;
}

static JSValue js_canvas_scale(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    double args[2];
    if (js_validate_doubles_args(ctx, argv, args, 2, 1))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    cairo_scale(context->ctx, args[0], args[1]);
    return JS_UNDEFINED;
}

static JSValue js_canvas_transform(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    double args[6];
    if (js_validate_doubles_args(ctx, argv, args, 6, 1))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    cairo_matrix_t matrix;
    cairo_matrix_init(&matrix, args[0], args[1], args[2], args[3], args[4], args[5]);
    cairo_transform(context->ctx, &matrix);
    return JS_UNDEFINED;
}

static JSValue js_canvas_reset_transform(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    cairo_identity_matrix(context->ctx);
    return JS_UNDEFINED;
}

static JSValue js_canvas_fill_rect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    RECT_ARGS;
    cairo_rectangle(context->ctx, x, y, width, height);
    cairo_fill(context->ctx);
    return JS_UNDEFINED;
}

static JSValue js_canvas_fill_text(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    double args[2];
    if (js_validate_doubles_args(ctx, argv, args, 2, 2))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    const char *text = JS_ToCString(ctx, argv[1]);
    cairo_move_to(context->ctx, args[0], args[1]);
    cairo_show_text(context->ctx, text);
    JS_FreeCString(ctx, text);
    return JS_UNDEFINED;
}

static JSValue js_canvas_measure_text(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    const char *text = JS_ToCString(ctx, argv[1]);
    cairo_text_extents_t extents;
    cairo_text_extents(context->ctx, text, &extents);
    JS_FreeCString(ctx, text);
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "xBearing", JS_NewFloat64(ctx, extents.x_bearing));
    JS_SetPropertyStr(ctx, obj, "yBearing", JS_NewFloat64(ctx, extents.y_bearing));
    JS_SetPropertyStr(ctx, obj, "xAdvance", JS_NewFloat64(ctx, extents.x_advance));
    JS_SetPropertyStr(ctx, obj, "yAdvance", JS_NewFloat64(ctx, extents.y_advance));
    JS_SetPropertyStr(ctx, obj, "width", JS_NewFloat64(ctx, extents.width));
    JS_SetPropertyStr(ctx, obj, "height", JS_NewFloat64(ctx, extents.height));
    return obj;
}

static JSValue js_canvas_get_image_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int sx;
    int sy;
    int sw;
    int sh;
    int cw;
    size_t length;
    uint32_t *buffer = (uint32_t *)JS_GetArrayBuffer(ctx, &length, argv[0]);
    if (JS_ToInt32(ctx, &sx, argv[1]) ||
        JS_ToInt32(ctx, &sy, argv[2]) ||
        JS_ToInt32(ctx, &sw, argv[3]) ||
        JS_ToInt32(ctx, &sh, argv[4]) ||
        JS_ToInt32(ctx, &cw, argv[5]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }

    // Create a new ArrayBuffer with managed data
    size_t size = sw * sh * 4;
    uint32_t *new_buffer = js_malloc(ctx, size);
    if (!new_buffer)
    {
        JS_ThrowOutOfMemory(ctx);
        return JS_EXCEPTION;
    }

    // Fill the buffer with some data
    memset(new_buffer, 0, size);
    for (int y = 0; y < sh; y++)
    {
        for (int x = 0; x < sw; x++)
        {
            new_buffer[(y * sw) + x] = buffer[(y * cw) + x];
        }
    }

    // Create the ArrayBuffer object
    return JS_NewArrayBuffer(ctx, (uint8_t *)new_buffer, size, NULL, NULL, 0);
}

static JSValue js_canvas_put_image_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int dx;
    int dy;
    int dirty_x;
    int dirty_y;
    int dirty_width;
    int dirty_height;
    int cw;
    size_t source_length;
    size_t dest_length;
    uint32_t *source_buffer = (uint32_t *)JS_GetArrayBuffer(ctx, &source_length, argv[0]);
    uint32_t *dest_buffer = (uint32_t *)JS_GetArrayBuffer(ctx, &dest_length, argv[1]);
    if (JS_ToInt32(ctx, &dx, argv[2]) ||
        JS_ToInt32(ctx, &dy, argv[3]) ||
        JS_ToInt32(ctx, &dirty_x, argv[4]) ||
        JS_ToInt32(ctx, &dirty_y, argv[5]) ||
        JS_ToInt32(ctx, &dirty_width, argv[6]) ||
        JS_ToInt32(ctx, &dirty_height, argv[7]) ||
        JS_ToInt32(ctx, &cw, argv[8]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    int dest_x;
    int dest_y;
    for (int y = dirty_y; y < dirty_height; y++)
    {
        for (int x = dirty_x; x < dirty_width; x++)
        {
            dest_x = dx + x;
            dest_y = dy + y;
            dest_buffer[(dest_y * cw) + dest_x] = source_buffer[(y * dirty_width) + x];
        }
    }
    return JS_UNDEFINED;
}

static void finalizer_canvas_context_2d(JSRuntime *rt, JSValue val)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque(val, nx_canvas_context_class_id);
    if (context)
    {
        cairo_destroy(context->ctx);
        cairo_surface_destroy(context->surface);
        js_free_rt(rt, context->data);
        js_free_rt(rt, context);
    }
}

nx_canvas_context_2d_t *nx_get_canvas_context_2d(JSContext *ctx, JSValueConst obj)
{
    return JS_GetOpaque2(ctx, obj, nx_canvas_context_class_id);
}

static const JSCFunctionListEntry function_list[] = {
    JS_CFUNC_DEF("canvasNewContext", 0, js_canvas_new_context),
    JS_CFUNC_DEF("canvasGetLineDash", 0, js_canvas_get_line_dash),
    JS_CFUNC_DEF("canvasSetLineDash", 0, js_canvas_set_line_dash),
    JS_CFUNC_DEF("canvasGetLineWidth", 0, js_canvas_get_line_width),
    JS_CFUNC_DEF("canvasSetLineWidth", 0, js_canvas_set_line_width),
    JS_CFUNC_DEF("canvasSetSourceRgba", 0, js_canvas_set_source_rgba),
    JS_CFUNC_DEF("canvasSetFont", 0, js_canvas_set_font),
    JS_CFUNC_DEF("canvasBeginPath", 0, js_canvas_begin_path),
    JS_CFUNC_DEF("canvasClosePath", 0, js_canvas_close_path),
    JS_CFUNC_DEF("canvasFill", 0, js_canvas_fill),
    JS_CFUNC_DEF("canvasStroke", 0, js_canvas_stroke),
    JS_CFUNC_DEF("canvasMoveTo", 0, js_canvas_move_to),
    JS_CFUNC_DEF("canvasLineTo", 0, js_canvas_line_to),
    JS_CFUNC_DEF("canvasRect", 0, js_canvas_rect),
    JS_CFUNC_DEF("canvasRotate", 0, js_canvas_rotate),
    JS_CFUNC_DEF("canvasTranslate", 0, js_canvas_translate),
    JS_CFUNC_DEF("canvasTransform", 0, js_canvas_transform),
    JS_CFUNC_DEF("canvasResetTransform", 0, js_canvas_reset_transform),
    JS_CFUNC_DEF("canvasScale", 0, js_canvas_scale),
    JS_CFUNC_DEF("canvasFillRect", 0, js_canvas_fill_rect),
    JS_CFUNC_DEF("canvasFillText", 0, js_canvas_fill_text),
    JS_CFUNC_DEF("canvasMeasureText", 0, js_canvas_measure_text),
    JS_CFUNC_DEF("canvasGetImageData", 0, js_canvas_get_image_data),
    JS_CFUNC_DEF("canvasPutImageData", 0, js_canvas_put_image_data)};

void nx_init_canvas(JSContext *ctx, JSValueConst native_obj)
{
    JSRuntime *rt = JS_GetRuntime(ctx);

    JS_NewClassID(&nx_canvas_context_class_id);
    JSClassDef font_face_class = {
        "nx_canvas_context_2d_t",
        .finalizer = finalizer_canvas_context_2d,
    };
    JS_NewClass(rt, nx_canvas_context_class_id, &font_face_class);

    JS_SetPropertyFunctionList(ctx, native_obj, function_list, countof(function_list));
}
