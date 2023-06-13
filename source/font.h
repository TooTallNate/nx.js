#pragma once
#include <quickjs/quickjs.h>
#include <cairo-ft.h>
#include <ft2build.h>

typedef struct
{
    FT_Face ft_face;
    cairo_font_face_t *cairo_font;
} nx_font_face_t;

nx_font_face_t *nx_get_font_face(JSContext *ctx, JSValueConst obj);
void nx_init_font(JSContext *ctx, JSValueConst native_obj);
