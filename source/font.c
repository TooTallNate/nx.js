#include <switch.h>
#include <harfbuzz/hb-ot.h>
#include "types.h"
#include "font.h"

static JSClassID nx_font_face_class_id;

JSValue nx_new_font_face_(JSContext *ctx, u8* data, size_t bytes)
{
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	nx_font_face_t *context = js_mallocz(ctx, sizeof(nx_font_face_t));
	if (!context) {
		return JS_EXCEPTION;
	}

	context->font_buffer = js_malloc(ctx, bytes);
	memcpy(context->font_buffer, data, bytes);

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

static JSValue nx_new_font_face(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	size_t bytes;
	FT_Byte *font_data = JS_GetArrayBuffer(ctx, &bytes, argv[0]);
	return nx_new_font_face_(ctx, font_data, bytes);
}

int nx_load_system_font(JSContext *ctx)
{
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	PlFontData font;
	Result rc = plGetSharedFontByType(&font, PlSharedFontType_Standard);
	if (R_FAILED(rc))
	{
		JS_ThrowTypeError(ctx, "Failed to load system font");
		return 1;
	}
	JSValue system_font = nx_new_font_face_(ctx, font.address, font.size);
	if (JS_IsException(system_font)) {
		return 1;
	}
	nx_ctx->system_font = JS_DupValue(ctx, system_font);
	return 0;
}

void finalizer_font_face_(JSRuntime *rt, nx_font_face_t *font) {
	if (font)
	{
		if (font->hb_font)
		{
			hb_font_destroy(font->hb_font);
		}
		if (font->cairo_font)
		{
			cairo_font_face_destroy(font->cairo_font);
		}
		if (font->ft_face)
		{
			FT_Done_Face(font->ft_face);
		}
		js_free_rt(rt, font->font_buffer);
		js_free_rt(rt, font);
	}
}

static void finalizer_font_face(JSRuntime *rt, JSValue val)
{
	nx_font_face_t *font = JS_GetOpaque(val, nx_font_face_class_id);
	finalizer_font_face_(rt, font);
}

nx_font_face_t *nx_get_font_face(JSContext *ctx, JSValueConst obj)
{
	return JS_GetOpaque2(ctx, obj, nx_font_face_class_id);
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("fontFaceNew", 0, nx_new_font_face),
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
