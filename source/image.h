#pragma once
#include "types.h"

enum ImageFormat
{
    FORMAT_PNG,
    FORMAT_JPEG,
    FORMAT_WEBP,
    FORMAT_UNKNOWN
};

typedef struct
{
    enum ImageFormat format;
    uint8_t *buffer;
    cairo_surface_t *surface;
} nx_image_t;

nx_image_t *nx_get_image(JSContext *ctx, JSValueConst obj);

void nx_init_image(JSContext *ctx, JSValueConst native_obj);
