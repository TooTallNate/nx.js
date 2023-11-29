#pragma once
#include <cairo.h>
#include "types.h"

/**
 * `Screen` / `OffscreenCanvas` / `Image` / `ImageBitmap`
 */
typedef struct
{
    uint32_t width;
    uint32_t height;
    uint8_t *data;
    cairo_surface_t *surface;
} nx_canvas_t;

nx_canvas_t *nx_get_canvas(JSContext *ctx, JSValueConst obj);

typedef struct {
    double r;
    double g;
    double b;
    double a;
} nx_rgba_t;

/**
 * `CanvasRenderingContext2D` / `OffscreenCanvasRenderingContext2D`
 */
typedef struct
{
    nx_canvas_t *canvas;
    cairo_t *ctx;
    cairo_path_t *path;
    FT_Face ft_face;
    double global_alpha;
    nx_rgba_t fill_style;
    nx_rgba_t stroke_style;
} nx_canvas_context_2d_t;

nx_canvas_context_2d_t *nx_get_canvas_context_2d(JSContext *ctx, JSValueConst obj);

void nx_init_canvas(JSContext *ctx, JSValueConst init_obj);
