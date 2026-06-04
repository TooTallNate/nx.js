#pragma once
#include "types.h"
#include <ft2build.h>
#include <harfbuzz/hb-ft.h>
#include <harfbuzz/hb.h>

#include "include/core/SkRefCnt.h"
#include "include/core/SkTypeface.h"

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define FREETYPE_VERSION_STR                                                   \
	STR(FREETYPE_MAJOR) "." STR(FREETYPE_MINOR) "." STR(FREETYPE_PATCH)

typedef struct {
	FT_Face ft_face;
	hb_font_t *hb_font;
	// Skia typeface built from the same font bytes (font_buffer). Replaces the
	// former cairo_font_face_t. Glyph IDs from HarfBuzz shaping (which uses the
	// same FT_Face) index into this typeface for SkCanvas text drawing.
	sk_sp<SkTypeface> sk_typeface;
	FT_Byte *font_buffer;
} nx_font_face_t;

nx_font_face_t *nx_get_font_face(v8::Isolate *iso, v8::Local<v8::Value> obj);
void nx_init_font(v8::Isolate *iso, v8::Local<v8::Object> init_obj);
