#include "compression.h"
#include "async.h"
#include "util.h"
#include <errno.h>

#define CHUNK 16384 // 16KB chunks

static JSClassID nx_compress_class_id;
static JSClassID nx_decompress_class_id;

typedef struct {
	int err;
	nx_compress_t *context;
	JSValue data_val;
	uint8_t *data;
	size_t size;
	uint8_t *result;
	size_t result_size;
} nx_compress_write_async_t;

typedef struct {
	int err;
	nx_compress_t *context;
	uint8_t *result;
	size_t result_size;
} nx_compress_flush_async_t;

typedef struct {
	int err;
	nx_decompress_t *context;
	JSValue data_val;
	uint8_t *data;
	size_t size;
	uint8_t *result;
	size_t result_size;
} nx_decompress_write_async_t;

typedef struct {
	int err;
	nx_decompress_t *context;
	uint8_t *result;
	size_t result_size;
} nx_decompress_flush_async_t;

static void free_array_buffer(JSRuntime *rt, void *opaque, void *ptr) {
	free(ptr);
}

nx_compression_format_t nx_compression_format_from_string(const char *str) {
	if (strcmp(str, "deflate") == 0) {
		return NX_COMPRESSION_FORMAT_DEFLATE;
	} else if (strcmp(str, "deflate-raw") == 0) {
		return NX_COMPRESSION_FORMAT_DEFLATE_RAW;
	} else if (strcmp(str, "gzip") == 0) {
		return NX_COMPRESSION_FORMAT_GZIP;
	} else if (strcmp(str, "zstd") == 0) {
		return NX_COMPRESSION_FORMAT_ZSTD;
	}
	return NX_COMPRESSION_FORMAT_UNKNOWN;
}

int nx_compression_zlib_window_bits(nx_compression_format_t format) {
	switch (format) {
	case NX_COMPRESSION_FORMAT_DEFLATE:
		// 15 = default window size (32K)
		return 15;
	case NX_COMPRESSION_FORMAT_DEFLATE_RAW:
		// -15 = raw deflate (negative value means no header/trailer)
		return -15;
	case NX_COMPRESSION_FORMAT_GZIP:
		// 15 + 16 = 31 (16 tells zlib to create gzip header/trailer)
		return 15 + 16;
	default:
		return -1;
	}
}

static void finalizer_compress(JSRuntime *rt, JSValue val) {
	nx_compress_t *context = JS_GetOpaque(val, nx_compress_class_id);
	if (context) {
		if (context->format == NX_COMPRESSION_FORMAT_DEFLATE ||
			context->format == NX_COMPRESSION_FORMAT_DEFLATE_RAW ||
			context->format == NX_COMPRESSION_FORMAT_GZIP) {
			if (context->zstream) {
				deflateEnd(context->zstream); // Clean up the zlib stream
				js_free_rt(rt, context->zstream);
			}
		} else if (context->format == NX_COMPRESSION_FORMAT_ZSTD) {
			if (context->cctx) {
				ZSTD_freeCCtx(context->cctx);
			}
		}
		js_free_rt(rt, context);
	}
}

static void finalizer_decompress(JSRuntime *rt, JSValue val) {
	nx_decompress_t *context = JS_GetOpaque(val, nx_decompress_class_id);
	if (context) {
		if (context->format == NX_COMPRESSION_FORMAT_DEFLATE ||
			context->format == NX_COMPRESSION_FORMAT_DEFLATE_RAW ||
			context->format == NX_COMPRESSION_FORMAT_GZIP) {
			if (context->zstream) {
				inflateEnd(context->zstream); // Clean up the zlib stream
				js_free_rt(rt, context->zstream);
			}
		} else if (context->format == NX_COMPRESSION_FORMAT_ZSTD) {
			if (context->dctx) {
				ZSTD_freeDCtx(context->dctx);
			}
		}
		js_free_rt(rt, context);
	}
}

#pragma region CompressionStream

static JSValue nx_compress_new(JSContext *ctx, JSValueConst this_val, int argc,
							   JSValueConst *argv) {
	nx_compress_t *context = js_mallocz(ctx, sizeof(nx_compress_t));
	if (!context)
		return JS_EXCEPTION;

	const char *format = JS_ToCString(ctx, argv[0]);
	if (!format) {
		js_free(ctx, context);
		return JS_EXCEPTION;
	}

	context->format = nx_compression_format_from_string(format);
	if (context->format == NX_COMPRESSION_FORMAT_UNKNOWN) {
		JS_ThrowTypeError(ctx, "Invalid compression format: %s", format);
		js_free(ctx, context);
		JS_FreeCString(ctx, format);
		return JS_EXCEPTION;
	}

	if (context->format == NX_COMPRESSION_FORMAT_DEFLATE ||
		context->format == NX_COMPRESSION_FORMAT_DEFLATE_RAW ||
		context->format == NX_COMPRESSION_FORMAT_GZIP) {
		// Init zlib stream
		z_stream *stream = js_mallocz(ctx, sizeof(z_stream));
		if (!stream) {
			js_free(ctx, context);
			JS_FreeCString(ctx, format);
			return JS_EXCEPTION;
		}
		stream->zalloc = Z_NULL;
		stream->zfree = Z_NULL;
		stream->opaque = Z_NULL;

		int level = 8;
		int window_bits = nx_compression_zlib_window_bits(context->format);
		int ret = deflateInit2(stream, level, Z_DEFLATED, window_bits, 8,
							   Z_DEFAULT_STRATEGY);
		if (ret != Z_OK) {
			JS_ThrowInternalError(
				ctx, "Failed to initialize %s stream (error code: %d)", format,
				ret);
			js_free(ctx, context);
			JS_FreeCString(ctx, format);
			return JS_EXCEPTION;
		}

		context->zstream = stream;
	} else if (context->format == NX_COMPRESSION_FORMAT_ZSTD) {
		context->cctx = ZSTD_createCCtx();
		if (!context->cctx) {
			JS_ThrowInternalError(ctx, "ZSTD_createCCtx() returned NULL");
			js_free(ctx, context);
			JS_FreeCString(ctx, format);
			return JS_EXCEPTION;
		}
	}

	JS_FreeCString(ctx, format);

	JSValue obj = JS_NewObjectClass(ctx, nx_compress_class_id);
	if (JS_IsException(obj)) {
		js_free(ctx, context);
		return obj;
	}

	JS_SetOpaque(obj, context);
	return obj;
}

void nx_compress_write_do(nx_work_t *req) {
	nx_compress_write_async_t *data = (nx_compress_write_async_t *)req->data;
	data->result = NULL;
	data->result_size = 0;

	if (data->context->format == NX_COMPRESSION_FORMAT_GZIP ||
		data->context->format == NX_COMPRESSION_FORMAT_DEFLATE ||
		data->context->format == NX_COMPRESSION_FORMAT_DEFLATE_RAW) {
		z_stream *stream = data->context->zstream;
		unsigned char out[CHUNK];
		int ret;

		stream->avail_in = data->size;
		stream->next_in = data->data;

		// compress until deflate stream ends
		do {
			stream->avail_out = CHUNK;
			stream->next_out = out;

			ret = deflate(stream, Z_NO_FLUSH);
			if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
				if (data->result) {
					free(data->result);
				}
				data->err = ret;
				return;
			}

			// Calculate how many bytes we got
			int have = CHUNK - stream->avail_out;

			// Only reallocate and copy if we have data
			if (have > 0) {
				// Reallocate our result buffer and append new data
				unsigned char *new_result =
					realloc(data->result, data->result_size + have);
				if (new_result == NULL) {
					free(data->result);
					deflateEnd(stream);
					data->err = Z_MEM_ERROR;
					return;
				}
				data->result = new_result;
				memcpy(data->result + data->result_size, out, have);
				data->result_size += have;
			}
		} while (stream->avail_in > 0);
	} else if (data->context->format == NX_COMPRESSION_FORMAT_ZSTD) {
		ZSTD_inBuffer input = {.src = data->data, .size = data->size, .pos = 0};

		// compress chunks until done
		while (input.pos < input.size) {
			// Allocate or expand result buffer
			size_t size = ZSTD_CStreamOutSize();
			void *new_result = realloc(data->result, data->result_size + size);
			if (!new_result) {
				if (data->result) {
					free(data->result);
				}
				ZSTD_freeCCtx(data->context->cctx);
				data->context->cctx = NULL;
				data->err = ENOMEM;
				return;
			}
			data->result = new_result;

			ZSTD_outBuffer output = {.dst = (char *)data->result +
											data->result_size,
									 .size = size,
									 .pos = 0};

			size_t ret =
				ZSTD_compressStream(data->context->cctx, &output, &input);
			fprintf(stderr, "zstd: %lu\n", ret);

			if (ZSTD_isError(ret)) {
				if (data->result) {
					free(data->result);
				}
				ZSTD_freeCCtx(data->context->cctx);
				data->context->cctx = NULL;
				data->err = -4;
				return;
			}

			data->result_size += output.pos;

			fprintf(stderr, "input.pos: %lu\n", input.pos);
			fprintf(stderr, "input.size: %lu\n", input.size);
			fprintf(stderr, "output.pos: %lu\n", output.pos);
			fprintf(stderr, "output.size: %lu\n", output.size);
		}

		// Shrink buffer to actual size
		// TODO: remove?
		if (data->result_size > 0) {
			void *final_result = realloc(data->result, data->result_size);
			if (final_result) {
				data->result = final_result;
			}
		}
	}
}

JSValue nx_compress_write_cb(JSContext *ctx, nx_work_t *req) {
	nx_compress_write_async_t *data = (nx_compress_write_async_t *)req->data;
	JS_FreeValue(ctx, data->data_val);

	if (data->err) {
		JSValue err = JS_NewError(ctx);
		JS_DefinePropertyValueStr(ctx, err, "message",
								  JS_NewString(ctx, strerror(data->err)),
								  JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
		return JS_Throw(ctx, err);
	}

	return JS_NewArrayBuffer(ctx, data->result, data->result_size,
							 free_array_buffer, NULL, false);
}

static JSValue nx_compress_write(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	NX_INIT_WORK_T(nx_compress_write_async_t);
	data->context = JS_GetOpaque2(ctx, argv[0], nx_compress_class_id);
	if (!data->context) {
		return JS_EXCEPTION;
	}

	data->data = NX_GetBufferSource(ctx, &data->size, argv[1]);
	if (!data->data) {
		js_free(ctx, data);
		return JS_EXCEPTION;
	}
	data->data_val = JS_DupValue(ctx, argv[1]);
	return nx_queue_async(ctx, req, nx_compress_write_do, nx_compress_write_cb);
}

void nx_compress_flush_do(nx_work_t *req) {
	nx_compress_flush_async_t *data = (nx_compress_flush_async_t *)req->data;

	if (data->context->format == NX_COMPRESSION_FORMAT_GZIP ||
		data->context->format == NX_COMPRESSION_FORMAT_DEFLATE ||
		data->context->format == NX_COMPRESSION_FORMAT_DEFLATE_RAW) {
		z_stream *stream = data->context->zstream;
		unsigned char out[CHUNK];
		int ret;

		// Initialize result buffer
		data->result = NULL;
		data->result_size = 0;

		// compress until deflate stream ends
		do {
			stream->avail_out = CHUNK;
			stream->next_out = out;

			ret = deflate(stream, Z_FINISH);
			if (ret != Z_OK && ret != Z_STREAM_END) {
				deflateEnd(stream);
				if (data->result) {
					free(data->result);
				}
				data->err = ret;
				return;
			}

			// Calculate how many bytes we got
			int have = CHUNK - stream->avail_out;

			// Reallocate our result buffer and append new data
			unsigned char *new_result =
				realloc(data->result, data->result_size + have);
			if (new_result == NULL) {
				free(data->result);
				deflateEnd(stream);
				data->err = Z_MEM_ERROR;
				return;
			}
			data->result = new_result;
			memcpy(data->result + data->result_size, out, have);
			data->result_size += have;
		} while (ret != Z_STREAM_END);

		// Clean up
		if (ret != Z_STREAM_END) {
			deflateEnd(stream);
		}
	} else if (data->context->format == NX_COMPRESSION_FORMAT_ZSTD) {
		// Initialize result buffer
		data->result = NULL;
		data->result_size = 0;

		// Create empty input buffer since we're just flushing
		ZSTD_inBuffer input = {.src = NULL, .size = 0, .pos = 0};

		// Keep flushing until no more output is produced
		size_t remaining;
		do {
			// Allocate or expand result buffer
			size_t size = ZSTD_CStreamOutSize();
			void *new_result = realloc(data->result, data->result_size + size);
			if (!new_result) {
				if (data->result) {
					free(data->result);
				}
				ZSTD_freeCCtx(data->context->cctx);
				data->context->cctx = NULL;
				data->err = ENOMEM;
				return;
			}
			data->result = new_result;

			// Set up output buffer for this chunk
			ZSTD_outBuffer output = {.dst = (char *)data->result +
											data->result_size,
									 .size = size,
									 .pos = 0};

			// Flush the compressor
			remaining = ZSTD_compressStream2(data->context->cctx, &output,
											 &input, ZSTD_e_end);
			if (ZSTD_isError(remaining)) {
				free(data->result);
				ZSTD_freeCCtx(data->context->cctx);
				data->context->cctx = NULL;
				data->err = EINVAL;
				return;
			}

			data->result_size += output.pos;
		} while (remaining > 0);
	}
}

JSValue nx_compress_flush_cb(JSContext *ctx, nx_work_t *req) {
	nx_compress_flush_async_t *data = (nx_compress_flush_async_t *)req->data;

	if (data->err) {
		JSValue err = JS_NewError(ctx);
		JS_DefinePropertyValueStr(ctx, err, "message",
								  JS_NewString(ctx, strerror(data->err)),
								  JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
		return JS_Throw(ctx, err);
	}

	return data->result_size > 0
			   ? JS_NewArrayBuffer(ctx, data->result, data->result_size,
								   free_array_buffer, NULL, false)
			   : JS_NULL;
}

static JSValue nx_compress_flush(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	NX_INIT_WORK_T(nx_compress_flush_async_t);
	data->context = JS_GetOpaque2(ctx, argv[0], nx_compress_class_id);
	if (!data->context) {
		return JS_EXCEPTION;
	}
	return nx_queue_async(ctx, req, nx_compress_flush_do, nx_compress_flush_cb);
}

#pragma endregion

#pragma region DecompressionStream

static JSValue nx_decompress_new(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	nx_decompress_t *context = js_mallocz(ctx, sizeof(nx_decompress_t));
	if (!context)
		return JS_EXCEPTION;

	const char *format = JS_ToCString(ctx, argv[0]);
	if (!format) {
		js_free(ctx, context);
		return JS_EXCEPTION;
	}

	int window_bits = -1;
	if (strcmp(format, "deflate") == 0) {
		context->format = NX_COMPRESSION_FORMAT_DEFLATE;
		// 15 for max window bits
		window_bits = 15;
	} else if (strcmp(format, "deflate-raw") == 0) {
		context->format = NX_COMPRESSION_FORMAT_DEFLATE;
		// -15 for raw deflate
		window_bits = -15;
	} else if (strcmp(format, "gzip") == 0) {
		context->format = NX_COMPRESSION_FORMAT_GZIP;
		// 15+16 for gzip format
		window_bits = 15 + 16;
	} else if (strcmp(format, "zstd") == 0) {
		context->format = NX_COMPRESSION_FORMAT_ZSTD;
		context->dctx = ZSTD_createDCtx();
		if (!context->dctx) {
			JS_ThrowInternalError(ctx, "ZSTD_createDCtx() returned NULL");
			js_free(ctx, context);
			JS_FreeCString(ctx, format);
			return JS_EXCEPTION;
		}
	} else {
		JS_ThrowTypeError(ctx, "Invalid compression format: %s", format);
		js_free(ctx, context);
		JS_FreeCString(ctx, format);
		return JS_EXCEPTION;
	}

	if (context->format == NX_COMPRESSION_FORMAT_DEFLATE ||
		context->format == NX_COMPRESSION_FORMAT_DEFLATE_RAW ||
		context->format == NX_COMPRESSION_FORMAT_GZIP) {
		// Init zlib stream
		z_stream *stream = js_mallocz(ctx, sizeof(z_stream));
		if (!stream) {
			js_free(ctx, context);
			JS_FreeCString(ctx, format);
			return JS_EXCEPTION;
		}
		stream->zalloc = Z_NULL;
		stream->zfree = Z_NULL;
		stream->opaque = Z_NULL;

		int ret = inflateInit2(stream, window_bits);
		if (ret != Z_OK) {
			JS_ThrowInternalError(
				ctx, "Failed to initialize %s stream (error code: %d)", format,
				ret);
			js_free(ctx, context);
			JS_FreeCString(ctx, format);
			return JS_EXCEPTION;
		}

		context->zstream = stream;
	}

	JS_FreeCString(ctx, format);

	JSValue obj = JS_NewObjectClass(ctx, nx_decompress_class_id);
	if (JS_IsException(obj)) {
		js_free(ctx, context);
		return obj;
	}

	JS_SetOpaque(obj, context);
	return obj;
}

void nx_decompress_write_do(nx_work_t *req) {
	nx_decompress_write_async_t *data =
		(nx_decompress_write_async_t *)req->data;
	data->result = NULL;
	data->result_size = 0;

	if (data->context->format == NX_COMPRESSION_FORMAT_GZIP ||
		data->context->format == NX_COMPRESSION_FORMAT_DEFLATE ||
		data->context->format == NX_COMPRESSION_FORMAT_DEFLATE_RAW) {
		z_stream *stream = data->context->zstream;
		unsigned char out[CHUNK];
		int ret;

		stream->avail_in = data->size;
		stream->next_in = data->data;

		// Decompress until deflate stream ends
		do {
			stream->avail_out = CHUNK;
			stream->next_out = out;

			ret = inflate(stream, Z_NO_FLUSH);
			if (ret != Z_OK && ret != Z_STREAM_END) {
				if (data->result) {
					free(data->result);
				}
				data->err = ret;
				return;
			}

			// Calculate how many bytes we got
			int have = CHUNK - stream->avail_out;

			// Reallocate our result buffer and append new data
			unsigned char *new_result =
				realloc(data->result, data->result_size + have);
			if (new_result == NULL) {
				free(data->result);
				inflateEnd(stream);
				data->err = Z_MEM_ERROR;
				return;
			}
			data->result = new_result;
			memcpy(data->result + data->result_size, out, have);
			data->result_size += have;
		} while (ret != Z_STREAM_END);
	} else if (data->context->format == NX_COMPRESSION_FORMAT_ZSTD) {
		ZSTD_inBuffer input = {.src = data->data, .size = data->size, .pos = 0};

		// Decompress chunks until done
		while (input.pos < input.size) {
			// Allocate or expand result buffer
			size_t size = ZSTD_DStreamOutSize();
			void *new_result = realloc(data->result, data->result_size + size);
			if (!new_result) {
				if (data->result) {
					free(data->result);
				}
				ZSTD_freeDCtx(data->context->dctx);
				data->context->dctx = NULL;
				data->err = ENOMEM;
				return;
			}
			data->result = new_result;

			ZSTD_outBuffer output = {.dst = (char *)data->result +
											data->result_size,
									 .size = size,
									 .pos = 0};

			size_t ret =
				ZSTD_decompressStream(data->context->dctx, &output, &input);

			if (ZSTD_isError(ret)) {
				if (data->result) {
					free(data->result);
				}
				ZSTD_freeDCtx(data->context->dctx);
				data->context->dctx = NULL;
				data->err = -4;
				return;
			}

			data->result_size += output.pos;
		}

		// Shrink buffer to actual size
		// TODO: remove?
		if (data->result_size > 0) {
			void *final_result = realloc(data->result, data->result_size);
			if (final_result) {
				data->result = final_result;
			}
		}
	}
}

JSValue nx_decompress_write_cb(JSContext *ctx, nx_work_t *req) {
	nx_decompress_write_async_t *data =
		(nx_decompress_write_async_t *)req->data;
	JS_FreeValue(ctx, data->data_val);

	if (data->err) {
		JSValue err = JS_NewError(ctx);
		JS_DefinePropertyValueStr(ctx, err, "message",
								  JS_NewString(ctx, strerror(data->err)),
								  JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
		return JS_Throw(ctx, err);
	}

	return JS_NewArrayBuffer(ctx, data->result, data->result_size,
							 free_array_buffer, NULL, false);
}

static JSValue nx_decompress_write(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	NX_INIT_WORK_T(nx_decompress_write_async_t);
	data->context = JS_GetOpaque2(ctx, argv[0], nx_decompress_class_id);
	if (!data->context) {
		return JS_EXCEPTION;
	}

	data->data = NX_GetBufferSource(ctx, &data->size, argv[1]);
	if (!data->data) {
		js_free(ctx, data);
		return JS_EXCEPTION;
	}
	data->data_val = JS_DupValue(ctx, argv[1]);
	return nx_queue_async(ctx, req, nx_decompress_write_do,
						  nx_decompress_write_cb);
}

void nx_decompress_flush_do(nx_work_t *req) {
	nx_decompress_flush_async_t *data =
		(nx_decompress_flush_async_t *)req->data;

	if (data->context->format == NX_COMPRESSION_FORMAT_GZIP ||
		data->context->format == NX_COMPRESSION_FORMAT_DEFLATE ||
		data->context->format == NX_COMPRESSION_FORMAT_DEFLATE_RAW) {
		z_stream *stream = data->context->zstream;
		unsigned char out[CHUNK];
		int ret;

		// Initialize result buffer
		data->result = NULL;
		data->result_size = 0;

		// Decompress until deflate stream ends
		do {
			stream->avail_out = CHUNK;
			stream->next_out = out;

			ret = inflate(stream, Z_FINISH);
			if (ret != Z_OK && ret != Z_STREAM_END) {
				inflateEnd(stream);
				if (data->result) {
					free(data->result);
				}
				data->err = ret;
				return;
			}

			// Calculate how many bytes we got
			int have = CHUNK - stream->avail_out;

			// Reallocate our result buffer and append new data
			unsigned char *new_result =
				realloc(data->result, data->result_size + have);
			if (new_result == NULL) {
				free(data->result);
				inflateEnd(stream);
				data->err = Z_MEM_ERROR;
				return;
			}
			data->result = new_result;
			memcpy(data->result + data->result_size, out, have);
			data->result_size += have;
		} while (ret != Z_STREAM_END);

		// Clean up
		if (ret != Z_STREAM_END) {
			inflateEnd(stream);
		}
	} else if (data->context->format == NX_COMPRESSION_FORMAT_ZSTD) {
		// Initialize result buffer
		data->result = NULL;
		data->result_size = 0;

		// Create empty input buffer since we're just flushing
		ZSTD_inBuffer input = {.src = NULL, .size = 0, .pos = 0};

		// Keep flushing until no more output is produced
		size_t remaining;
		do {
			// Allocate or expand result buffer
			size_t size = ZSTD_DStreamOutSize();
			void *new_result = realloc(data->result, data->result_size + size);
			if (!new_result) {
				if (data->result) {
					free(data->result);
				}
				ZSTD_freeDCtx(data->context->dctx);
				data->context->dctx = NULL;
				data->err = ENOMEM;
				return;
			}
			data->result = new_result;

			// Set up output buffer for this chunk
			ZSTD_outBuffer output = {.dst = (char *)data->result +
											data->result_size,
									 .size = size,
									 .pos = 0};

			// Flush the decompressor
			remaining =
				ZSTD_decompressStream(data->context->dctx, &output, &input);
			if (ZSTD_isError(remaining)) {
				free(data->result);
				ZSTD_freeDCtx(data->context->dctx);
				data->context->dctx = NULL;
				data->err = EINVAL;
				return;
			}

			data->result_size += output.pos;
		} while (remaining > 0);
	}
}

JSValue nx_decompress_flush_cb(JSContext *ctx, nx_work_t *req) {
	nx_decompress_flush_async_t *data =
		(nx_decompress_flush_async_t *)req->data;

	if (data->err) {
		JSValue err = JS_NewError(ctx);
		JS_DefinePropertyValueStr(ctx, err, "message",
								  JS_NewString(ctx, strerror(data->err)),
								  JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
		return JS_Throw(ctx, err);
	}

	return data->result_size > 0
			   ? JS_NewArrayBuffer(ctx, data->result, data->result_size,
								   free_array_buffer, NULL, false)
			   : JS_NULL;
}

static JSValue nx_decompress_flush(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	NX_INIT_WORK_T(nx_decompress_flush_async_t);
	data->context = JS_GetOpaque2(ctx, argv[0], nx_decompress_class_id);
	if (!data->context) {
		return JS_EXCEPTION;
	}
	return nx_queue_async(ctx, req, nx_decompress_flush_do,
						  nx_decompress_flush_cb);
}

#pragma endregion

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("compressNew", 1, nx_compress_new),
	JS_CFUNC_DEF("compressWrite", 1, nx_compress_write),
	JS_CFUNC_DEF("compressFlush", 1, nx_compress_flush),
	JS_CFUNC_DEF("decompressNew", 1, nx_decompress_new),
	JS_CFUNC_DEF("decompressWrite", 1, nx_decompress_write),
	JS_CFUNC_DEF("decompressFlush", 1, nx_decompress_flush),
};

void nx_init_compression(JSContext *ctx, JSValueConst init_obj) {
	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_compress_class_id);
	JSClassDef compress_class = {
		"CompressHandle",
		.finalizer = finalizer_compress,
	};
	JS_NewClass(rt, nx_compress_class_id, &compress_class);

	JS_NewClassID(rt, &nx_decompress_class_id);
	JSClassDef decompress_class = {
		"DecompressHandle",
		.finalizer = finalizer_decompress,
	};
	JS_NewClass(rt, nx_decompress_class_id, &decompress_class);

	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
