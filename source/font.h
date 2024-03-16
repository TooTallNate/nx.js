#pragma once
#include <cairo.h>
#include <quickjs.h>
#include <ft2build.h>
#include <harfbuzz/hb.h>
#include <harfbuzz/hb-ft.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define FREETYPE_VERSION_STR STR(FREETYPE_MAJOR) "." STR(FREETYPE_MINOR) "." STR(FREETYPE_PATCH)

typedef struct nx_font_face_s
{
	FT_Face ft_face;
	hb_font_t *hb_font;
	cairo_font_face_t *cairo_font;
	FT_Byte *font_buffer;
} nx_font_face_t;

int nx_load_system_font(JSContext *ctx);
nx_font_face_t *nx_get_font_face(JSContext *ctx, JSValueConst obj);
void nx_init_font(JSContext *ctx, JSValueConst init_obj);
