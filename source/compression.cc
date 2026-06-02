#include "async.h"
#include "error.h"
#include "types.h"
#include "util.h"
#include "wrap.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <zstd.h>

using namespace v8;

namespace {

#define CHUNK 16384

typedef enum {
	NX_FMT_UNKNOWN,
	NX_FMT_DEFLATE,
	NX_FMT_DEFLATE_RAW,
	NX_FMT_GZIP,
	NX_FMT_ZSTD,
} fmt_t;

typedef struct {
	fmt_t format;
	union {
		z_stream *zstream;
		ZSTD_CCtx *cctx;
	};
} nx_compress_t;

typedef struct {
	fmt_t format;
	union {
		z_stream *zstream;
		ZSTD_DCtx *dctx;
	};
} nx_decompress_t;

bool is_zlib(fmt_t f) {
	return f == NX_FMT_DEFLATE || f == NX_FMT_DEFLATE_RAW || f == NX_FMT_GZIP;
}

fmt_t format_from_string(const char *str) {
	if (!strcmp(str, "deflate"))
		return NX_FMT_DEFLATE;
	if (!strcmp(str, "deflate-raw"))
		return NX_FMT_DEFLATE_RAW;
	if (!strcmp(str, "gzip"))
		return NX_FMT_GZIP;
	if (!strcmp(str, "zstd"))
		return NX_FMT_ZSTD;
	return NX_FMT_UNKNOWN;
}

int zlib_window_bits(fmt_t format) {
	switch (format) {
	case NX_FMT_DEFLATE:
		return 15;
	case NX_FMT_DEFLATE_RAW:
		return -15;
	case NX_FMT_GZIP:
		return 15 + 16;
	default:
		return -1;
	}
}

void free_compress(nx_compress_t *c) {
	if (is_zlib(c->format)) {
		if (c->zstream) {
			deflateEnd(c->zstream);
			free(c->zstream);
		}
	} else if (c->format == NX_FMT_ZSTD && c->cctx) {
		ZSTD_freeCCtx(c->cctx);
	}
	free(c);
}

void free_decompress(nx_decompress_t *c) {
	if (is_zlib(c->format)) {
		if (c->zstream) {
			inflateEnd(c->zstream);
			free(c->zstream);
		}
	} else if (c->format == NX_FMT_ZSTD && c->dctx) {
		ZSTD_freeDCtx(c->dctx);
	}
	free(c);
}

// ---- result -> ArrayBuffer / throw ----
MaybeLocal<Value> result_or_throw(Isolate *iso, int err, uint8_t **result,
                                  size_t result_size, bool null_if_empty) {
	if (err) {
		if (*result) {
			free(*result);
			*result = nullptr;
		}
		iso->ThrowException(Exception::Error(nx_str(iso, strerror(err))));
		return MaybeLocal<Value>();
	}
	if (null_if_empty && result_size == 0) {
		return Null(iso).As<Value>();
	}
	uint8_t *buf = *result;
	*result = nullptr;
	std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
	    buf, result_size, [](void *p, size_t, void *) { free(p); }, nullptr);
	return ArrayBuffer::New(iso, std::move(bs)).As<Value>();
}

// ---- CompressionStream ----
void nx_compress_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value format(iso, info[0]);
	if (!*format)
		return;
	fmt_t fmt = format_from_string(*format);
	if (fmt == NX_FMT_UNKNOWN) {
		nx_throw(iso, "Invalid compression format");
		return;
	}
	nx_compress_t *context =
	    (nx_compress_t *)calloc(1, sizeof(nx_compress_t));
	context->format = fmt;
	if (is_zlib(fmt)) {
		z_stream *stream = (z_stream *)calloc(1, sizeof(z_stream));
		stream->zalloc = Z_NULL;
		stream->zfree = Z_NULL;
		stream->opaque = Z_NULL;
		int ret = deflateInit2(stream, 8, Z_DEFLATED, zlib_window_bits(fmt), 8,
		                       Z_DEFAULT_STRATEGY);
		if (ret != Z_OK) {
			free(stream);
			free(context);
			nx_throw(iso, "Failed to initialize deflate stream");
			return;
		}
		context->zstream = stream;
	} else {
		context->cctx = ZSTD_createCCtx();
		if (!context->cctx) {
			free(context);
			nx_throw(iso, "ZSTD_createCCtx() returned NULL");
			return;
		}
	}
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_compress_t>(iso, obj, context, free_compress);
	info.GetReturnValue().Set(obj);
}

typedef struct {
	int err;
	nx_compress_t *context;
	Global<Value> data_val;
	uint8_t *data;
	size_t size;
	uint8_t *result;
	size_t result_size;
} compress_write_t;

void compress_write_do(nx_work_t *req) {
	compress_write_t *data = (compress_write_t *)req->data;
	data->result = NULL;
	data->result_size = 0;
	if (is_zlib(data->context->format)) {
		z_stream *stream = data->context->zstream;
		unsigned char out[CHUNK];
		stream->avail_in = data->size;
		stream->next_in = data->data;
		do {
			stream->avail_out = CHUNK;
			stream->next_out = out;
			int ret = deflate(stream, Z_NO_FLUSH);
			if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
				if (data->result)
					free(data->result);
				data->err = ret;
				return;
			}
			int have = CHUNK - stream->avail_out;
			if (have > 0) {
				unsigned char *nr =
				    (unsigned char *)realloc(data->result,
				                             data->result_size + have);
				if (!nr) {
					free(data->result);
					data->err = Z_MEM_ERROR;
					return;
				}
				data->result = nr;
				memcpy(data->result + data->result_size, out, have);
				data->result_size += have;
			}
		} while (stream->avail_in > 0);
	} else { // zstd
		ZSTD_inBuffer input = {data->data, data->size, 0};
		while (input.pos < input.size) {
			size_t size = ZSTD_CStreamOutSize();
			void *nr = realloc(data->result, data->result_size + size);
			if (!nr) {
				if (data->result)
					free(data->result);
				data->err = ENOMEM;
				return;
			}
			data->result = (uint8_t *)nr;
			ZSTD_outBuffer output = {data->result + data->result_size, size,
			                         0};
			size_t ret =
			    ZSTD_compressStream(data->context->cctx, &output, &input);
			if (ZSTD_isError(ret)) {
				free(data->result);
				data->err = -4;
				return;
			}
			data->result_size += output.pos;
		}
	}
}

MaybeLocal<Value> compress_write_cb(Isolate *iso, nx_work_t *req) {
	compress_write_t *data = (compress_write_t *)req->data;
	data->data_val.Reset();
	return result_or_throw(iso, data->err, &data->result, data->result_size,
	                       false);
}

void nx_compress_write(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_compress_t *context = nx::Unwrap<nx_compress_t>(info[0]);
	if (!context)
		return;
	size_t size = 0;
	uint8_t *buf = NX_GetBufferSource(iso, &size, info[1]);
	if (!buf) {
		nx_throw(iso, "expected ArrayBuffer");
		return;
	}
	nx_work_t *req = new nx_work_t();
	compress_write_t *data = new compress_write_t();
	req->data = data;
	data->context = context;
	data->data = buf;
	data->size = size;
	data->data_val.Reset(iso, info[1]);
	info.GetReturnValue().Set(
	    nx_queue_async(iso, req, compress_write_do, compress_write_cb));
}

typedef struct {
	int err;
	nx_compress_t *context;
	uint8_t *result;
	size_t result_size;
} compress_flush_t;

void compress_flush_do(nx_work_t *req) {
	compress_flush_t *data = (compress_flush_t *)req->data;
	data->result = NULL;
	data->result_size = 0;
	if (is_zlib(data->context->format)) {
		z_stream *stream = data->context->zstream;
		unsigned char out[CHUNK];
		int ret;
		do {
			stream->avail_out = CHUNK;
			stream->next_out = out;
			ret = deflate(stream, Z_FINISH);
			if (ret != Z_OK && ret != Z_STREAM_END) {
				if (data->result)
					free(data->result);
				data->err = ret;
				return;
			}
			int have = CHUNK - stream->avail_out;
			unsigned char *nr =
			    (unsigned char *)realloc(data->result,
			                             data->result_size + have);
			if (!nr) {
				free(data->result);
				data->err = Z_MEM_ERROR;
				return;
			}
			data->result = nr;
			memcpy(data->result + data->result_size, out, have);
			data->result_size += have;
		} while (ret != Z_STREAM_END);
	} else { // zstd
		ZSTD_inBuffer input = {NULL, 0, 0};
		size_t remaining;
		do {
			size_t size = ZSTD_CStreamOutSize();
			void *nr = realloc(data->result, data->result_size + size);
			if (!nr) {
				if (data->result)
					free(data->result);
				data->err = ENOMEM;
				return;
			}
			data->result = (uint8_t *)nr;
			ZSTD_outBuffer output = {data->result + data->result_size, size,
			                         0};
			remaining = ZSTD_compressStream2(data->context->cctx, &output,
			                                 &input, ZSTD_e_end);
			if (ZSTD_isError(remaining)) {
				free(data->result);
				data->err = EINVAL;
				return;
			}
			data->result_size += output.pos;
		} while (remaining > 0);
	}
}

MaybeLocal<Value> compress_flush_cb(Isolate *iso, nx_work_t *req) {
	compress_flush_t *data = (compress_flush_t *)req->data;
	return result_or_throw(iso, data->err, &data->result, data->result_size,
	                       true);
}

void nx_compress_flush(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	NX_INIT_WORK_T(compress_flush_t);
	data->context = nx::Unwrap<nx_compress_t>(info[0]);
	if (!data->context) {
		free(data);
		delete req;
		return;
	}
	info.GetReturnValue().Set(
	    nx_queue_async(iso, req, compress_flush_do, compress_flush_cb));
}

// ---- DecompressionStream ----
void nx_decompress_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value format(iso, info[0]);
	if (!*format)
		return;
	fmt_t fmt = format_from_string(*format);
	if (fmt == NX_FMT_UNKNOWN) {
		nx_throw(iso, "Invalid compression format");
		return;
	}
	nx_decompress_t *context =
	    (nx_decompress_t *)calloc(1, sizeof(nx_decompress_t));
	context->format = fmt;
	if (is_zlib(fmt)) {
		z_stream *stream = (z_stream *)calloc(1, sizeof(z_stream));
		stream->zalloc = Z_NULL;
		stream->zfree = Z_NULL;
		stream->opaque = Z_NULL;
		int ret = inflateInit2(stream, zlib_window_bits(fmt));
		if (ret != Z_OK) {
			free(stream);
			free(context);
			nx_throw(iso, "Failed to initialize inflate stream");
			return;
		}
		context->zstream = stream;
	} else {
		context->dctx = ZSTD_createDCtx();
		if (!context->dctx) {
			free(context);
			nx_throw(iso, "ZSTD_createDCtx() returned NULL");
			return;
		}
	}
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_decompress_t>(iso, obj, context, free_decompress);
	info.GetReturnValue().Set(obj);
}

typedef struct {
	int err;
	nx_decompress_t *context;
	Global<Value> data_val;
	uint8_t *data;
	size_t size;
	uint8_t *result;
	size_t result_size;
} decompress_write_t;

void decompress_write_do(nx_work_t *req) {
	decompress_write_t *data = (decompress_write_t *)req->data;
	data->result = NULL;
	data->result_size = 0;
	if (is_zlib(data->context->format)) {
		z_stream *stream = data->context->zstream;
		unsigned char out[CHUNK];
		int ret;
		stream->avail_in = data->size;
		stream->next_in = data->data;
		do {
			stream->avail_out = CHUNK;
			stream->next_out = out;
			ret = inflate(stream, Z_NO_FLUSH);
			if (ret == Z_BUF_ERROR)
				break;
			if (ret != Z_OK && ret != Z_STREAM_END) {
				if (data->result)
					free(data->result);
				data->err = ret;
				return;
			}
			int have = CHUNK - stream->avail_out;
			unsigned char *nr =
			    (unsigned char *)realloc(data->result,
			                             data->result_size + have);
			if (!nr) {
				free(data->result);
				data->err = Z_MEM_ERROR;
				return;
			}
			data->result = nr;
			memcpy(data->result + data->result_size, out, have);
			data->result_size += have;
		} while (ret != Z_STREAM_END);
	} else { // zstd
		ZSTD_inBuffer input = {data->data, data->size, 0};
		while (input.pos < input.size) {
			size_t size = ZSTD_DStreamOutSize();
			void *nr = realloc(data->result, data->result_size + size);
			if (!nr) {
				if (data->result)
					free(data->result);
				data->err = ENOMEM;
				return;
			}
			data->result = (uint8_t *)nr;
			ZSTD_outBuffer output = {data->result + data->result_size, size,
			                         0};
			size_t ret =
			    ZSTD_decompressStream(data->context->dctx, &output, &input);
			if (ZSTD_isError(ret)) {
				free(data->result);
				data->err = -4;
				return;
			}
			data->result_size += output.pos;
		}
	}
}

MaybeLocal<Value> decompress_write_cb(Isolate *iso, nx_work_t *req) {
	decompress_write_t *data = (decompress_write_t *)req->data;
	data->data_val.Reset();
	return result_or_throw(iso, data->err, &data->result, data->result_size,
	                       false);
}

void nx_decompress_write(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_decompress_t *context = nx::Unwrap<nx_decompress_t>(info[0]);
	if (!context)
		return;
	size_t size = 0;
	uint8_t *buf = NX_GetBufferSource(iso, &size, info[1]);
	if (!buf) {
		nx_throw(iso, "expected ArrayBuffer");
		return;
	}
	nx_work_t *req = new nx_work_t();
	decompress_write_t *data = new decompress_write_t();
	req->data = data;
	data->context = context;
	data->data = buf;
	data->size = size;
	data->data_val.Reset(iso, info[1]);
	info.GetReturnValue().Set(
	    nx_queue_async(iso, req, decompress_write_do, decompress_write_cb));
}

typedef struct {
	int err;
	nx_decompress_t *context;
	uint8_t *result;
	size_t result_size;
} decompress_flush_t;

void decompress_flush_do(nx_work_t *req) {
	decompress_flush_t *data = (decompress_flush_t *)req->data;
	data->result = NULL;
	data->result_size = 0;
	if (is_zlib(data->context->format)) {
		z_stream *stream = data->context->zstream;
		unsigned char out[CHUNK];
		int ret;
		do {
			stream->avail_out = CHUNK;
			stream->next_out = out;
			ret = inflate(stream, Z_FINISH);
			if (ret != Z_OK && ret != Z_STREAM_END) {
				if (data->result)
					free(data->result);
				data->err = ret;
				return;
			}
			int have = CHUNK - stream->avail_out;
			unsigned char *nr =
			    (unsigned char *)realloc(data->result,
			                             data->result_size + have);
			if (!nr) {
				free(data->result);
				data->err = Z_MEM_ERROR;
				return;
			}
			data->result = nr;
			memcpy(data->result + data->result_size, out, have);
			data->result_size += have;
		} while (ret != Z_STREAM_END);
	} else { // zstd
		ZSTD_inBuffer input = {NULL, 0, 0};
		size_t size = ZSTD_DStreamOutSize();
		void *nr = realloc(data->result, data->result_size + size);
		if (!nr) {
			if (data->result)
				free(data->result);
			data->err = ENOMEM;
			return;
		}
		data->result = (uint8_t *)nr;
		ZSTD_outBuffer output = {data->result + data->result_size, size, 0};
		size_t ret =
		    ZSTD_decompressStream(data->context->dctx, &output, &input);
		if (ZSTD_isError(ret)) {
			free(data->result);
			data->err = EINVAL;
			return;
		}
		data->result_size += output.pos;
	}
}

MaybeLocal<Value> decompress_flush_cb(Isolate *iso, nx_work_t *req) {
	decompress_flush_t *data = (decompress_flush_t *)req->data;
	return result_or_throw(iso, data->err, &data->result, data->result_size,
	                       true);
}

void nx_decompress_flush(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	NX_INIT_WORK_T(decompress_flush_t);
	data->context = nx::Unwrap<nx_decompress_t>(info[0]);
	if (!data->context) {
		free(data);
		delete req;
		return;
	}
	info.GetReturnValue().Set(
	    nx_queue_async(iso, req, decompress_flush_do, decompress_flush_cb));
}

} // namespace

void nx_init_compression(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "compressNew", nx_compress_new);
	NX_SET_FUNC(init_obj, "compressWrite", nx_compress_write);
	NX_SET_FUNC(init_obj, "compressFlush", nx_compress_flush);
	NX_SET_FUNC(init_obj, "decompressNew", nx_decompress_new);
	NX_SET_FUNC(init_obj, "decompressWrite", nx_decompress_write);
	NX_SET_FUNC(init_obj, "decompressFlush", nx_decompress_flush);
}
