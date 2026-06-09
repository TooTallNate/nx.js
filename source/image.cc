#include "image.h"
#include "async.h"
#include "error.h"
#include "util.h"
#include "wrap.h"
#include <png.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <turbojpeg.h>
#include <webp/decode.h>

using namespace v8;

namespace {

struct buffer_state {
	uint8_t *ptr;
	size_t size;
};

void close_image(nx_image_t *image) {
	// Drop the memoized SkImage (built from a copy of the pixels). Ordering
	// vs. freeing `data` below doesn't strictly matter since the cache owns its
	// own copy, but release it up front to keep teardown tidy.
	nx_image_release_cache(image);
	if (image->data) {
		if (image->format == FORMAT_JPEG) {
			tjFree(image->data);
		} else {
			free(image->data);
		}
		image->data = NULL;
	}
	image->width = image->height = 0;
}

void user_read_data(png_structp png_ptr, png_bytep data, png_size_t length) {
	struct buffer_state *state =
	    (struct buffer_state *)png_get_io_ptr(png_ptr);
	memcpy(data, state->ptr, length);
	state->ptr += length;
}

enum ImageFormat identify_image_format(uint8_t *data, size_t size) {
	if (size >= 8 && !memcmp(data, "\211PNG\r\n\032\n", 8))
		return FORMAT_PNG;
	if (size >= 2 && !memcmp(data, "\377\330", 2))
		return FORMAT_JPEG;
	if (size >= 12 && !memcmp(data, "RIFF", 4) && !memcmp(data + 8, "WEBP", 4))
		return FORMAT_WEBP;
	return FORMAT_UNKNOWN;
}

void premultiply_alpha(uint8_t *image_data, int width, int height) {
	for (int i = 0; i < width * height; ++i) {
		uint8_t *pixel = &image_data[i * 4];
		uint8_t alpha = pixel[3];
		pixel[0] = (pixel[0] * alpha) / 255;
		pixel[1] = (pixel[1] * alpha) / 255;
		pixel[2] = (pixel[2] * alpha) / 255;
	}
}

uint8_t *decode_png(uint8_t *input, size_t input_size, u32 *width,
                    u32 *height) {
	png_structp png_ptr =
	    png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info_ptr = png_create_info_struct(png_ptr);
	struct buffer_state state = {input, input_size};
	png_set_read_fn(png_ptr, &state, user_read_data);
	png_read_info(png_ptr, info_ptr);
	*width = png_get_image_width(png_ptr, info_ptr);
	*height = png_get_image_height(png_ptr, info_ptr);
	png_set_bgr(png_ptr);
	png_set_expand(png_ptr);
	bool has_alpha =
	    png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGBA;
	if (!has_alpha)
		png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
	if (*width == 0 || *height == 0 || *width > 16384 || *height > 16384 ||
	    (size_t)(*width) > SIZE_MAX / 4 / (*height)) {
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return NULL;
	}
	uint8_t *image_data = (uint8_t *)malloc(4 * (size_t)(*width) * (*height));
	png_bytep *rows = (png_bytep *)malloc(sizeof(png_bytep) * (*height));
	if (!image_data || !rows) {
		free(image_data);
		free(rows);
		png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
		return NULL;
	}
	for (u32 i = 0; i < *height; ++i)
		rows[i] = image_data + i * 4 * (*width);
	png_read_image(png_ptr, rows);
	free(rows);
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	if (has_alpha)
		premultiply_alpha(image_data, *width, *height);
	return image_data;
}

uint8_t *decode_webp(uint8_t *webp_data, size_t data_size, int *width,
                     int *height) {
	uint8_t *bgra_data = WebPDecodeBGRA(webp_data, data_size, width, height);
	if (bgra_data == NULL)
		return NULL;
	premultiply_alpha(bgra_data, *width, *height);
	return bgra_data;
}

// ---- async decode ----
typedef struct {
	int err;
	const char *err_str;
	nx_image_t *image;
	Global<Value> image_val;
	Global<Value> buffer_val;
	uint8_t *input;
	size_t input_size;
} decode_image_t;

void nx_decode_image_do(nx_work_t *req) {
	decode_image_t *data = (decode_image_t *)req->data;
	data->image->format = identify_image_format(data->input, data->input_size);
	if (data->image->format == FORMAT_PNG) {
		data->image->data = decode_png(data->input, data->input_size,
		                               &data->image->width,
		                               &data->image->height);
	} else if (data->image->format == FORMAT_JPEG) {
		if (decode_jpeg(data->input, data->input_size, &data->image->data,
		                (int *)&data->image->width,
		                (int *)&data->image->height)) {
			data->err_str = tjGetErrorStr();
			return;
		}
	} else if (data->image->format == FORMAT_WEBP) {
		data->image->data = decode_webp(data->input, data->input_size,
		                                (int *)&data->image->width,
		                                (int *)&data->image->height);
	} else {
		data->err_str = "Unsupported image format";
		return;
	}
	if (data->image->data == NULL) {
		data->err_str = "Image decode was not initialized";
		return;
	}
	// `data->image->data` holds premultiplied BGRA; canvas.cc wraps it in an
	// SkImage at draw time. No persistent surface object needed.
}

MaybeLocal<Value> nx_decode_image_cb(Isolate *iso, nx_work_t *req) {
	decode_image_t *data = (decode_image_t *)req->data;
	data->image_val.Reset();
	data->buffer_val.Reset();
	if (data->err_str) {
		iso->ThrowException(Exception::Error(nx_str(iso, data->err_str)));
		return MaybeLocal<Value>();
	}
	return Undefined(iso).As<Value>();
}

void nx_image_decode(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_image_t *image = nx_get_image(iso, info[0]);
	if (!image)
		return;
	size_t size = 0;
	uint8_t *buf = NX_GetBufferSource(iso, &size, info[1]);
	if (!buf) {
		nx_throw(iso, "expected ArrayBuffer");
		return;
	}
	nx_work_t *req = new nx_work_t();
	decode_image_t *data = new decode_image_t();
	req->data = data;
	req->data_dtor = [](void *p) { delete static_cast<decode_image_t *>(p); };
	data->image = image;
	data->image_val.Reset(iso, info[0]);
	data->buffer_val.Reset(iso, info[1]);
	data->input = buf;
	data->input_size = size;
	info.GetReturnValue().Set(
	    nx_queue_async(iso, req, nx_decode_image_do, nx_decode_image_cb));
}

void free_image(nx_image_t *image) {
	close_image(image);
	free(image);
}

void nx_image_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	Local<Object> img = nx::NewWrapped(iso);
	nx_image_t *data = (nx_image_t *)calloc(1, sizeof(nx_image_t));
	nx::Wrap<nx_image_t>(iso, img, data, free_image);
	if (info.Length() == 2) {
		uint32_t w = 0, h = 0;
		if (!info[0]->Uint32Value(context).To(&w) ||
		    !info[1]->Uint32Value(context).To(&h))
			return;
		data->width = w;
		data->height = h;
		if (w == 0 || h == 0 || w > SIZE_MAX / 4 ||
		    (size_t)h > SIZE_MAX / ((size_t)w * 4)) {
			iso->ThrowException(
			    Exception::RangeError(nx_str(iso, "Image dimensions too large")));
			return;
		}
		data->data = (uint8_t *)calloc(1, (size_t)w * h * 4);
	}
	info.GetReturnValue().Set(img);
}

void nx_image_close(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_image_t *image = nx_get_image(iso, info[0]);
	if (image)
		close_image(image);
}

void nx_image_get_width(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_image_t *image = nx_get_image(iso, info.This());
	if (image)
		info.GetReturnValue().Set(Integer::NewFromUnsigned(iso, image->width));
}

void nx_image_get_height(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_image_t *image = nx_get_image(iso, info.This());
	if (image)
		info.GetReturnValue().Set(Integer::NewFromUnsigned(iso, image->height));
}

void nx_image_init_class(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	Local<Object> proto = info[0]
	                          .As<Object>()
	                          ->Get(context, nx_str(iso, "prototype"))
	                          .ToLocalChecked()
	                          .As<Object>();
	NX_DEF_GET(proto, "width", nx_image_get_width);
	NX_DEF_GET(proto, "height", nx_image_get_height);
}

} // namespace

// ---- shared, non-namespaced symbols ----

nx_image_t *nx_get_image(Isolate *iso, Local<Value> obj) {
	(void)iso;
	return nx::Unwrap<nx_image_t>(obj);
}

int decode_jpeg(uint8_t *jpegBuf, size_t jpegSize, uint8_t **output, int *width,
                int *height) {
	tjhandle handle = NULL;
	int subsamp, colorspace;
	int ret = -1;
	handle = tjInitDecompress();
	if (handle == NULL)
		goto cleanup;
	if (tjDecompressHeader3(handle, jpegBuf, jpegSize, width, height, &subsamp,
	                        &colorspace) == -1)
		goto cleanup;
	*output = tjAlloc((*width) * (*height) * tjPixelSize[TJPF_BGRA]);
	if (tjDecompress2(handle, jpegBuf, jpegSize, *output, *width, 0, *height,
	                  TJPF_BGRA, TJFLAG_FASTDCT) == -1) {
		tjFree(*output);
		*output = NULL;
		goto cleanup;
	}
	ret = 0;
cleanup:
	if (handle != NULL)
		tjDestroy(handle);
	return ret;
}

void nx_init_image(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "imageInit", nx_image_init_class);
	NX_SET_FUNC(init_obj, "imageNew", nx_image_new);
	NX_SET_FUNC(init_obj, "imageDecode", nx_image_decode);
	NX_SET_FUNC(init_obj, "imageClose", nx_image_close);
}
