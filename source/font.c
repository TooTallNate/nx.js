#include <switch.h>
#include "types.h"
#include "font.h"

static JSClassID js_font_face_class_id;

static JSValue js_new_font_face(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_context_t *nx_ctx = nx_get_context(ctx);
    nx_font_face_t *context = js_malloc(ctx, sizeof(nx_font_face_t));
    if (!context)
    {
        JS_ThrowOutOfMemory(ctx);
        return JS_EXCEPTION;
    }
    memset(context, 0, sizeof(nx_font_face_t));

    size_t bytes;
    FT_Byte *font_data = JS_GetArrayBuffer(ctx, &bytes, argv[0]);

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
    context->cairo_font = cairo_ft_font_face_create_for_ft_face(context->ft_face, 0);

    JSValue obj = JS_NewObjectClass(ctx, js_font_face_class_id);
    if (JS_IsException(obj))
    {
        free(context);
        return obj;
    }

    JS_SetOpaque(obj, context);

    return obj;
}

static JSValue js_get_system_font(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
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
    nx_font_face_t *context = JS_GetOpaque(val, js_font_face_class_id);
    printf("Finalizing font face");
    if (context)
    {
        FT_Done_Face(context->ft_face);
        cairo_font_face_destroy(context->cairo_font);
        js_free_rt(rt, context);
    }
}

nx_font_face_t *nx_get_font_face(JSContext *ctx, JSValueConst obj)
{
    return JS_GetOpaque2(ctx, obj, js_font_face_class_id);
}

static const JSCFunctionListEntry function_list[] = {
    JS_CFUNC_DEF("newFontFace", 0, js_new_font_face),
    JS_CFUNC_DEF("getSystemFont", 0, js_get_system_font)};

void nx_init_font(JSContext *ctx, JSValueConst native_obj)
{
    JSRuntime *rt = JS_GetRuntime(ctx);

    JS_NewClassID(&js_font_face_class_id);
    JSClassDef font_face_class = {
        "FontFace",
        .finalizer = finalizer_font_face,
    };
    JS_NewClass(rt, js_font_face_class_id, &font_face_class);

    JS_SetPropertyFunctionList(ctx, native_obj, function_list, sizeof(function_list) / sizeof(function_list[0]));
}
