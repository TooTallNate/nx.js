#include <switch.h>
#include <harfbuzz/hb-ot.h>
#include "types.h"
#include "font.h"
#include "error.h"

static JSClassID nx_font_face_class_id;

static JSValue nx_new_font_face(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	nx_font_face_t *context = js_mallocz(ctx, sizeof(nx_font_face_t));
	if (!context)
		return JS_EXCEPTION;

	size_t bytes;
	FT_Byte *font_data = JS_GetArrayBuffer(ctx, &bytes, argv[0]);
	context->font_buffer = js_malloc(ctx, bytes);
	memcpy(context->font_buffer, font_data, bytes);

	if (nx_ctx->ft_library == NULL)
	{
		// Initialize FreeType library
		FT_Init_FreeType(&nx_ctx->ft_library);
	}

	FT_New_Memory_Face(nx_ctx->ft_library,
					   context->font_buffer, /* first byte in memory */
					   bytes,				 /* size in bytes        */
					   0,					 /* face_index           */
					   &context->ft_face);

	// For CAIRO, load using FreeType
	context->cairo_font = cairo_ft_font_face_create_for_ft_face(context->ft_face, 0);

	// For Harfbuzz, load using OpenType (HarfBuzz FT does not support bitmap font)
	hb_blob_t *blob = hb_blob_create((const char *)context->font_buffer, bytes, 0, NULL, NULL);
	hb_face_t *face = hb_face_create(blob, 0);
	context->hb_font = hb_font_create(face);
	hb_ot_font_set_funcs(context->hb_font);
	hb_font_set_scale(context->hb_font, 30 * 64, 30 * 64);

	JSValue obj = JS_NewObjectClass(ctx, nx_font_face_class_id);
	if (JS_IsException(obj))
	{
		js_free(ctx, context);
		return obj;
	}

	JS_SetOpaque(obj, context);
	return obj;
}

static JSValue nx_get_system_font(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 type;
	if (JS_ToUint32(ctx, &type, argv[0]))
	{
		return JS_EXCEPTION;
	}
	PlFontData font;
	Result rc = plGetSharedFontByType(&font, (PlSharedFontType)type);
	if (R_FAILED(rc))
	{
		return nx_throw_libnx_error(ctx, rc, "plGetSharedFontByType()");
	}
	return JS_NewArrayBufferCopy(ctx, font.address, font.size);
}

static void finalizer_font_face(JSRuntime *rt, JSValue val)
{
	nx_font_face_t *context = JS_GetOpaque(val, nx_font_face_class_id);
	if (context)
	{
		if (context->hb_font)
		{
			hb_font_destroy(context->hb_font);
		}
		if (context->cairo_font)
		{
			cairo_font_face_destroy(context->cairo_font);
		}
		if (context->ft_face)
		{
			FT_Done_Face(context->ft_face);
		}
		js_free_rt(rt, context->font_buffer);
		js_free_rt(rt, context);
	}
}

nx_font_face_t *nx_get_font_face(JSContext *ctx, JSValueConst obj)
{
	return JS_GetOpaque2(ctx, obj, nx_font_face_class_id);
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("fontFaceNew", 0, nx_new_font_face),
	JS_CFUNC_DEF("getSystemFont", 0, nx_get_system_font),
};

void nx_init_font(JSContext *ctx, JSValueConst init_obj)
{
	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_font_face_class_id);
	JSClassDef font_face_class = {
		"FontFace",
		.finalizer = finalizer_font_face,
	};
	JS_NewClass(rt, nx_font_face_class_id, &font_face_class);

	JS_SetPropertyFunctionList(ctx, init_obj, function_list, countof(function_list));
}
