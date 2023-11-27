#include <switch.h>
#include "types.h"
#include "font.h"

static JSClassID nx_font_face_class_id;

static JSValue nx_new_font_face(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_context_t *nx_ctx = nx_get_context(ctx);
    nx_font_face_t *context = js_mallocz(ctx, sizeof(nx_font_face_t));
    if (!context)
        return JS_EXCEPTION;

    context->font_buffer = JS_DupValue(ctx, argv[0]);

    size_t bytes;
    FT_Byte *font_data = JS_GetArrayBuffer(ctx, &bytes, argv[0]);
    // printf("new font, %d bytes, %p font_data\n", bytes, font_data);

    if (nx_ctx->ft_library == NULL)
    {
        // Initialize FreeType library
        FT_Init_FreeType(&nx_ctx->ft_library);
    }

    FT_New_Memory_Face(nx_ctx->ft_library,
                       font_data, /* first byte in memory */
                       bytes,     /* size in bytes        */
                       0,         /* face_index           */
                       &context->ft_face);

    // Create a Cairo font face from the FreeType face
    context->cairo_font = cairo_ft_font_face_create_for_ft_face(context->ft_face, FT_LOAD_COLOR | FT_LOAD_RENDER);
    // printf("ft_face %p, cairo_font: %p\n", context->ft_face, context->cairo_font);

    JSValue obj = JS_NewObjectClass(ctx, nx_font_face_class_id);
    if (JS_IsException(obj))
    {
        js_free(ctx, context);
        return obj;
    }

    JS_SetOpaque(obj, context);
    return obj;
}

static JSValue nx_font_face_dispose(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_font_face_t *context = JS_GetOpaque2(ctx, this_val, nx_font_face_class_id);
    JS_FreeValue(ctx, context->font_buffer);
    if (context->ft_face)
    {
        FT_Done_Face(context->ft_face);
        context->ft_face = NULL;
    }
    if (context->cairo_font)
    {
        cairo_font_face_destroy(context->cairo_font);
        context->cairo_font = NULL;
    }
    return JS_UNDEFINED;
}

static JSValue nx_get_system_font(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    PlFontData font;
    Result rc = plGetSharedFontByType(&font, PlSharedFontType_Standard);
    if (R_FAILED(rc))
    {
        JS_ThrowTypeError(ctx, "Failed to load system font");
        return JS_EXCEPTION;
    }
    return JS_NewArrayBufferCopy(ctx, font.address, font.size);
}

static void finalizer_font_face(JSRuntime *rt, JSValue val)
{
    nx_font_face_t *context = JS_GetOpaque(val, nx_font_face_class_id);
    if (context)
    {
        JS_FreeValueRT(rt, context->font_buffer);
        if (context->ft_face)
        {
            FT_Done_Face(context->ft_face);
        }
        if (context->cairo_font)
        {
            cairo_font_face_destroy(context->cairo_font);
        }
        js_free_rt(rt, context);
    }
}

nx_font_face_t *nx_get_font_face(JSContext *ctx, JSValueConst obj)
{
    return JS_GetOpaque2(ctx, obj, nx_font_face_class_id);
}

static const JSCFunctionListEntry function_list[] = {
    JS_CFUNC_DEF("fontFaceNew", 0, nx_new_font_face),
    JS_CFUNC_DEF("fontFaceDispose", 0, nx_font_face_dispose),
    JS_CFUNC_DEF("getSystemFont", 0, nx_get_system_font)};

void nx_init_font(JSContext *ctx, JSValueConst init_obj)
{
    JSRuntime *rt = JS_GetRuntime(ctx);

    JS_NewClassID(&nx_font_face_class_id);
    JSClassDef font_face_class = {
        "FontFace",
        .finalizer = finalizer_font_face,
    };
    JS_NewClass(rt, nx_font_face_class_id, &font_face_class);

    JS_SetPropertyFunctionList(ctx, init_obj, function_list, countof(function_list));
}
