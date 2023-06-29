#include "font.h"
#include "canvas.h"

static JSClassID nx_canvas_context_class_id;

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

    // printf("1: %p\n", (void*)context);
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

static JSValue js_canvas_set_fill_style(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    double r;
    double g;
    double b;
    double a;
    if (JS_ToFloat64(ctx, &r, argv[1]) ||
        JS_ToFloat64(ctx, &g, argv[2]) ||
        JS_ToFloat64(ctx, &b, argv[3]) ||
        JS_ToFloat64(ctx, &a, argv[4]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    cairo_set_source_rgba(context->ctx, r / (double)255, g / (double)255, b / (double)255, a);
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
    double x;
    double y;
    if (JS_ToFloat64(ctx, &x, argv[1]) || JS_ToFloat64(ctx, &y, argv[2]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    cairo_translate(context->ctx, x, y);
    return JS_UNDEFINED;
}

static JSValue js_canvas_scale(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    double x;
    double y;
    if (JS_ToFloat64(ctx, &x, argv[1]) || JS_ToFloat64(ctx, &y, argv[2]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    cairo_scale(context->ctx, x, y);
    return JS_UNDEFINED;
}

static JSValue js_canvas_fill_rect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_canvas_context_2d_t *context = JS_GetOpaque2(ctx, argv[0], nx_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    double x;
    double y;
    double w;
    double h;
    if (JS_ToFloat64(ctx, &x, argv[1]) ||
        JS_ToFloat64(ctx, &y, argv[2]) ||
        JS_ToFloat64(ctx, &w, argv[3]) ||
        JS_ToFloat64(ctx, &h, argv[4]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    cairo_rectangle(
        context->ctx,
        x, y,
        w,
        h);
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
    double x;
    double y;
    if (JS_ToFloat64(ctx, &x, argv[2]) ||
        JS_ToFloat64(ctx, &y, argv[3]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    const char *text = JS_ToCString(ctx, argv[1]);
    cairo_move_to(context->ctx, x, y);
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
    JS_CFUNC_DEF("canvasSetLineWidth", 0, js_canvas_set_line_width),
    JS_CFUNC_DEF("canvasSetFillStyle", 0, js_canvas_set_fill_style),
    JS_CFUNC_DEF("canvasSetFont", 0, js_canvas_set_font),
    JS_CFUNC_DEF("canvasRotate", 0, js_canvas_rotate),
    JS_CFUNC_DEF("canvasTranslate", 0, js_canvas_translate),
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
