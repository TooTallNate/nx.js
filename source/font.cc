#include "font.h"
#include "error.h"
#include "types.h"
#include "util.h"
#include "wrap.h"
#include <harfbuzz/hb-ot.h>
#include <memory>
#include <new>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

#include "include/core/SkData.h"
#include "include/core/SkFontMgr.h"
#include "include/ports/SkFontMgr_empty.h"

using namespace v8;

namespace {

void free_font_face(nx_font_face_t *context) {
	if (context->hb_font)
		hb_font_destroy(context->hb_font);
	// Drop the SkTypeface ref before free() (which won't run the sk_sp dtor).
	context->sk_typeface.reset();
	if (context->ft_face)
		FT_Done_Face(context->ft_face);
	if (context->font_buffer)
		free(context->font_buffer);
	free(context);
}

void nx_new_font_face(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_context_t *ctx = nx_ctx(iso);

	size_t bytes = 0;
	FT_Byte *font_data = NX_GetBufferSource(iso, &bytes, info[0]);
	if (!font_data) {
		nx_throw(iso, "expected ArrayBuffer");
		return;
	}

	nx_font_face_t *context =
	    (nx_font_face_t *)nx_alloc(iso, sizeof(nx_font_face_t));
	if (!context)
		return;
	// Placement-new to value-initialize: the struct holds an sk_sp<SkTypeface>
	// (non-trivial), so memset() over it is UB (-Wclass-memaccess). The matching
	// teardown is free_font_face(), which resets the sk_sp before free().
	new (context) nx_font_face_t();
	context->font_buffer = (FT_Byte *)nx_alloc(iso, bytes);
	if (!context->font_buffer) {
		free(context);
		return;
	}
	memcpy(context->font_buffer, font_data, bytes);

	if (ctx->ft_library == NULL) {
		FT_Init_FreeType(&ctx->ft_library);
	}

	FT_Error ft_err = FT_New_Memory_Face(ctx->ft_library, context->font_buffer,
	                                     bytes, 0, &context->ft_face);
	if (ft_err) {
		free(context->font_buffer);
		free(context);
		nx_throw(iso, "FreeType: failed to load font face");
		return;
	}

	// Build the Skia typeface from the same font bytes. SkFontMgr_New_Custom_Empty
	// is a self-contained (no system fontconfig) FreeType-backed manager; the
	// resulting typeface's glyph IDs match HarfBuzz shaping over the same face.
	{
		sk_sp<SkData> data =
		    SkData::MakeWithoutCopy(context->font_buffer, bytes);
		sk_sp<SkFontMgr> mgr = SkFontMgr_New_Custom_Empty();
		context->sk_typeface = mgr->makeFromData(data);
	}
	if (!context->sk_typeface) {
		FT_Done_Face(context->ft_face);
		free(context->font_buffer);
		free(context);
		nx_throw(iso, "Skia: failed to create typeface");
		return;
	}

	hb_blob_t *blob = hb_blob_create((const char *)context->font_buffer, bytes,
	                                 HB_MEMORY_MODE_READONLY, NULL, NULL);
	hb_face_t *face = blob ? hb_face_create(blob, 0) : NULL;
	context->hb_font = face ? hb_font_create(face) : NULL;
	if (face)
		hb_face_destroy(face);
	if (blob)
		hb_blob_destroy(blob);
	if (!context->hb_font) {
		context->sk_typeface.reset();
		FT_Done_Face(context->ft_face);
		free(context->font_buffer);
		free(context);
		nx_throw(iso, "HarfBuzz: failed to create font");
		return;
	}
	hb_ot_font_set_funcs(context->hb_font);
	hb_font_set_scale(context->hb_font, 30 * 64, 30 * 64);

	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_font_face_t>(iso, obj, context, free_font_face);
	info.GetReturnValue().Set(obj);
}

void nx_get_system_font(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	uint32_t type = 0;
	if (!info[0]->Uint32Value(context).To(&type))
		return;
	PlFontData font;
	Result rc = plGetSharedFontByType(&font, (PlSharedFontType)type);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "plGetSharedFontByType");
		return;
	}
	// Copy the shared font bytes into an ArrayBuffer. Allocate the backing
	// store with a checked malloc (NOT ArrayBuffer::New(iso, size), which
	// FATALLY aborts the process on allocation failure): in the tight applet
	// memory regime this multi-MiB copy can fail, and the JS font loaders
	// catch the thrown error and fall back gracefully (e.g. the console
	// terminal falls back to the libnx PrintConsole).
	uint8_t *buffer = (uint8_t *)malloc(font.size);
	if (buffer == NULL) {
		nx_throw_oom(iso, font.size);
		return;
	}
	memcpy(buffer, font.address, font.size);
	std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
	    buffer, font.size, [](void *p, size_t, void *) { free(p); }, nullptr);
	info.GetReturnValue().Set(ArrayBuffer::New(iso, std::move(bs)));
}

} // namespace

nx_font_face_t *nx_get_font_face(Isolate *iso, Local<Value> obj) {
	(void)iso;
	return nx::Unwrap<nx_font_face_t>(obj);
}

void nx_init_font(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "fontFaceNew", nx_new_font_face);
	NX_SET_FUNC(init_obj, "getSystemFont", nx_get_system_font);
}
