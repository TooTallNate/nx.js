#pragma once
#include <cairo.h>
#include "types.h"

typedef struct
{
    uint32_t width;
    uint32_t height;
    uint8_t *data;
    cairo_t *ctx;
    cairo_surface_t *surface;
    cairo_path_t *path;
    // cairo_font_face_t *font;
    FT_Face ft_face;
} nx_canvas_context_2d_t;

nx_canvas_context_2d_t *nx_get_canvas_context_2d(JSContext *ctx, JSValueConst obj);
void nx_init_canvas(JSContext *ctx, JSValueConst native_obj);
