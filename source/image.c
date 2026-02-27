#include "image.h"
#include "async.h"
#include <cairo.h>
#include <png.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <turbojpeg.h>
#include <webp/decode.h>

static JSClassID nx_image_class_id;

typedef struct {
	int err;
	char *err_str;
	nx_image_t *image;
	JSValue image_val;
	JSValue buffer_val;
	uint8_t *input;
	size_t input_size;
} nx_decode_image_async_t;

struct buffer_state {
	uint8_t *ptr;
	size_t size;
};

nx_image_t *nx_get_image(JSContext *ctx, JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_image_class_id);
}

void close_image(JSRuntime *rt, nx_image_t *image) {
	if (image->surface) {
		cairo_surface_destroy(image->surface);
		image->surface = NULL;
	}

	if (image->data) {
		if (image->data_needs_js_free) {
			js_free_rt(rt, image->data);
		} else if (image->format == FORMAT_JPEG) {
			tjFree(image->data);
		} else {
			free(image->data);
		}
		image->data = NULL;
		image->data_needs_js_free = false;
	}

	image->width = image->height = 0;
}

void user_read_data(png_structp png_ptr, png_bytep data, png_size_t length) {
	struct buffer_state *state = (struct buffer_state *)png_get_io_ptr(png_ptr);
	memcpy(data, state->ptr, length);
	state->ptr += length;
}

enum ImageFormat identify_image_format(uint8_t *data, size_t size) {
	if (size >= 8 && !memcmp(data, "\211PNG\r\n\032\n", 8)) {
		return FORMAT_PNG;
	} else if (size >= 2 && !memcmp(data, "\377\330", 2)) {
		return FORMAT_JPEG;
	} else if (size >= 12 && !memcmp(data, "RIFF", 4) &&
			   !memcmp(data + 8, "WEBP", 4)) {
		return FORMAT_WEBP;
	}
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
	if (!has_alpha) {
		png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
	}

	uint8_t *image_data = malloc(4 * (*width) * (*height));
	png_bytep rows[*height];
	for (int i = 0; i < *height; ++i) {
		rows[i] = image_data + i * 4 * (*width);
	}

	png_read_image(png_ptr, rows);

	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

	if (has_alpha) {
		premultiply_alpha(image_data, *width, *height);
	}

	return image_data;
}

int decode_jpeg(uint8_t *jpegBuf, size_t jpegSize, uint8_t **output, int *width,
				int *height) {
	tjhandle handle = NULL;
	int subsamp, colorspace;
	int ret = -1;

	handle = tjInitDecompress();
	if (handle == NULL) {
		// printf("Error in tjInitDecompress(): %s\n", tjGetErrorStr());
		goto cleanup;
	}

	if (tjDecompressHeader3(handle, jpegBuf, jpegSize, width, height, &subsamp,
							&colorspace) == -1) {
		// printf("Error in tjDecompressHeader3(): %s\n", tjGetErrorStr());
		goto cleanup;
	}

	*output = tjAlloc((*width) * (*height) * tjPixelSize[TJPF_BGRA]);

	if (tjDecompress2(handle, jpegBuf, jpegSize, *output, *width, 0 /*pitch*/,
					  *height, TJPF_BGRA, TJFLAG_FASTDCT) == -1) {
		// printf("Error in tjDecompress2(): %s\n", tjGetErrorStr());
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

uint8_t *decode_webp(uint8_t *webp_data, size_t data_size, int *width,
					 int *height) {
	// Decode the WebP image data into a BGRA format
	uint8_t *bgra_data = WebPDecodeBGRA(webp_data, data_size, width, height);
	if (bgra_data == NULL) {
		// TODO: Handle error
		return NULL;
	}

	premultiply_alpha(bgra_data, *width, *height);

	return bgra_data;
}

void nx_decode_image_do(nx_work_t *req) {
	nx_decode_image_async_t *data = (nx_decode_image_async_t *)req->data;
	data->image->format = identify_image_format(data->input, data->input_size);
	if (data->image->format == FORMAT_PNG) {
		data->image->data =
			decode_png(data->input, data->input_size, &data->image->width,
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
	data->image->surface = cairo_image_surface_create_for_data(
		data->image->data, CAIRO_FORMAT_ARGB32, data->image->width,
		data->image->height, data->image->width * 4);
}

JSValue nx_decode_image_cb(JSContext *ctx, nx_work_t *req) {
	nx_decode_image_async_t *data = (nx_decode_image_async_t *)req->data;

	if (data->err) {
		JSValue err = JS_NewError(ctx);
		JS_DefinePropertyValueStr(ctx, err, "message",
								  JS_NewString(ctx, strerror(data->err)),
								  JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
		JS_FreeValue(ctx, data->image_val);
		JS_FreeValue(ctx, data->buffer_val);
		return JS_Throw(ctx, err);
	} else if (data->err_str) {
		JSValue err = JS_NewError(ctx);
		JS_DefinePropertyValueStr(ctx, err, "message",
								  JS_NewString(ctx, data->err_str),
								  JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
		JS_FreeValue(ctx, data->image_val);
		JS_FreeValue(ctx, data->buffer_val);
		return JS_Throw(ctx, err);
	}

	JS_FreeValue(ctx, data->image_val);
	JS_FreeValue(ctx, data->buffer_val);
	return JS_UNDEFINED;
}

JSValue nx_image_decode(JSContext *ctx, JSValueConst this_val, int argc,
						JSValueConst *argv) {
	NX_INIT_WORK_T(nx_decode_image_async_t);
	data->image = nx_get_image(ctx, argv[0]);
	data->image_val = JS_DupValue(ctx, argv[0]);
	data->buffer_val = JS_DupValue(ctx, argv[1]);
	data->input = JS_GetArrayBuffer(ctx, &data->input_size, data->buffer_val);
	return nx_queue_async(ctx, req, nx_decode_image_do, nx_decode_image_cb);
}

JSValue nx_image_new(JSContext *ctx, JSValueConst this_val, int argc,
					 JSValueConst *argv) {
	JSValue img = JS_NewObjectClass(ctx, nx_image_class_id);
	if (JS_IsException(img)) {
		return img;
	}
	nx_image_t *data = js_mallocz(ctx, sizeof(nx_image_t));
	if (!data) {
		return JS_EXCEPTION;
	}
	JS_SetOpaque(img, data);
	if (argc == 2) {
		if (JS_ToUint32(ctx, &data->width, argv[0]) ||
			JS_ToUint32(ctx, &data->height, argv[1])) {
			return JS_EXCEPTION;
		}
		// Overflow check before allocation
		if (data->width == 0 || data->height == 0 ||
			data->width > SIZE_MAX / 4 ||
			(size_t)data->height > SIZE_MAX / ((size_t)data->width * 4)) {
			return JS_ThrowRangeError(ctx, "Image dimensions too large");
		}
		// Width and height were specified, so allocate a backing store to use
		data->data = js_mallocz(ctx, (size_t)data->width * data->height * 4);
		data->data_needs_js_free = true;
		if (!data->data) {
			return JS_EXCEPTION;
		}
		data->surface = cairo_image_surface_create_for_data(
			data->data, CAIRO_FORMAT_ARGB32, data->width, data->height,
			data->width * 4);
	}
	return img;
}

JSValue nx_image_close(JSContext *ctx, JSValueConst this_val, int argc,
					   JSValueConst *argv) {
	nx_image_t *image = nx_get_image(ctx, argv[0]);
	if (!image) {
		return JS_EXCEPTION;
	}
	close_image(JS_GetRuntime(ctx), image);
	return JS_UNDEFINED;
}

static void finalizer_image(JSRuntime *rt, JSValue val) {
	nx_image_t *image = JS_GetOpaque(val, nx_image_class_id);
	if (image) {
		close_image(rt, image);
		js_free_rt(rt, image);
	}
}

static JSValue nx_image_get_width(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_image_t *image = nx_get_image(ctx, this_val);
	if (!image)
		return JS_EXCEPTION;
	return JS_NewUint32(ctx, image->width);
}

static JSValue nx_image_get_height(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_image_t *image = nx_get_image(ctx, this_val);
	if (!image)
		return JS_EXCEPTION;
	return JS_NewUint32(ctx, image->height);
}

static JSValue nx_image_init_class(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_GET(proto, "width", nx_image_get_width);
	NX_DEF_GET(proto, "height", nx_image_get_height);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("imageInit", 0, nx_image_init_class),
	JS_CFUNC_DEF("imageNew", 0, nx_image_new),
	JS_CFUNC_DEF("imageDecode", 0, nx_image_decode),
	JS_CFUNC_DEF("imageClose", 0, nx_image_close),
};

void nx_init_image(JSContext *ctx, JSValueConst init_obj) {
	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_image_class_id);
	JSClassDef image_class = {
		"Image",
		.finalizer = finalizer_image,
	};
	JS_NewClass(rt, nx_image_class_id, &image_class);

	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
