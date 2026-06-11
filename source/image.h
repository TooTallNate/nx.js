#pragma once
#include "types.h"
#include <png.h>
#include <webp/decode.h>

enum ImageFormat { FORMAT_PNG, FORMAT_JPEG, FORMAT_WEBP, FORMAT_UNKNOWN };

// Decoded image: `data` is premultiplied BGRA (byte order B,G,R,A), width*4
// stride. canvas.cc wraps `data` in an SkImage at draw time.
//
// `cached_sk_image` memoizes that wrapped SkImage across draw calls. Without
// it, every `drawImage(<this image>)` rebuilt a fresh SkImage from the raw
// pixels (`RasterFromPixmapCopy`), which on the GPU backend forced a full
// re-upload of the source pixmap to a GPU texture EVERY call (measured:
// ~8ms for a 1024² source, ~25ms for 2048², independent of destination
// size — i.e. upload-bound, not fill-bound). Memoizing the SkImage gives it
// a stable identity so Ganesh caches the texture upload, collapsing repeat
// draws of the same source to ~0. It's an opaque `sk_sp<SkImage>*` here so
// this C-style header doesn't have to pull in the Skia C++ headers; canvas.cc
// owns its construction and `close_image` releases it via
// `nx_image_release_cache`.
typedef struct {
	u32 width;
	u32 height;
	u8 *data;
	bool data_needs_js_free; // (legacy flag; now always malloc/free or tjFree)
	enum ImageFormat format;
	void *cached_sk_image; // sk_sp<SkImage>* — lazily built in canvas.cc
} nx_image_t;

// Release an image's cached SkImage (if any). Defined in canvas.cc where the
// Skia type is available; called from image.cc's close_image.
void nx_image_release_cache(nx_image_t *image);

// Retrieve the native image wrapped by a JS object (or nullptr). Shared with
// canvas.cc / irs.cc.
nx_image_t *nx_get_image(v8::Isolate *iso, v8::Local<v8::Value> obj);

int decode_jpeg(uint8_t *jpegBuf, size_t jpegSize, uint8_t **output, int *width,
                int *height);

void nx_init_image(v8::Isolate *iso, v8::Local<v8::Object> init_obj);
