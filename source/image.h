#pragma once
#include "types.h"
#include <png.h>
#include <webp/decode.h>

#define LIBTURBOJPEG_VERSION "2.1.2"

enum ImageFormat
{
	FORMAT_PNG,
	FORMAT_JPEG,
	FORMAT_WEBP,
	FORMAT_UNKNOWN
};

typedef struct
{
	u32 width;
	u32 height;
	u8 *data;
	bool data_needs_js_free;
	cairo_surface_t *surface;
	enum ImageFormat format;
} nx_image_t;

nx_image_t *nx_get_image(JSContext *ctx, JSValueConst obj);

void nx_init_image(JSContext *ctx, JSValueConst init_obj);
