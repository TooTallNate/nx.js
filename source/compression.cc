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

// ===========================================================================
// Fused file decompression (read + decompress in one thread-pool dispatch).
//
// The JS pattern `Switch.file(path).slice(a,b).stream().pipeThrough(new
// DecompressionStream(fmt))` otherwise costs, per chunk: a $.fread dispatch, a
// $.decompressWrite dispatch, the web-streams-polyfill pipe's ~10 promises,
// and several fresh ArrayBuffers — plus the zstd output realloc-grow spikes.
// On a memory-constrained applet that is both slow (~0.6 MB/s) and fragile
// (OOM partway through a large stream).
//
// This fused path opens the fd once, and each pull() reads a fixed input chunk
// AND decompresses it into a fixed reused output buffer in the SAME worker
// dispatch, returning one output ArrayBuffer (a single copy). No realloc, no
// per-chunk fd/decompress round-trip split, no polyfill pipe. The result is
// bounded peak memory (two fixed buffers + one output copy) and far fewer
// dispatches.
// ===========================================================================

// Input is read in fixed chunks; output is accumulated up to a per-pull cap so
// each pull() does as much read+decompress work as possible in ONE thread-pool
// dispatch (fewer dispatches = less per-chunk overhead, which dominates
// throughput — see the on-device profiling). The output cap is chosen by the
// caller per memory regime (larger in application mode); the input chunk is
// fixed and small since compressed input is a fraction of output.
#define NX_FUSED_IN_CHUNK (256 * 1024)
#define NX_FUSED_OUT_MIN (256 * 1024)
#define NX_FUSED_OUT_MAX (8 * 1024 * 1024)

typedef struct {
	fmt_t format;
	union {
		z_stream *zstream;
		ZSTD_DCtx *dctx;
	};
	FILE *file;
	uint64_t remaining; // bytes of compressed input left to read (end - pos)
	uint8_t *in;        // fixed input buffer (NX_FUSED_IN_CHUNK)
	size_t in_size;     // valid bytes currently in `in`
	size_t in_pos;      // consumed offset within `in`
	uint8_t *out;       // fixed output buffer (out_cap bytes)
	size_t out_cap;     // per-pull output capacity (regime-chosen)
	bool eof_in;        // no more compressed input to read from the file
	bool finished;      // decompression stream fully drained
} nx_decompress_file_t;

void free_decompress_file(nx_decompress_file_t *c) {
	if (!c)
		return;
	if (is_zlib(c->format)) {
		if (c->zstream) {
			inflateEnd(c->zstream);
			free(c->zstream);
		}
	} else if (c->format == NX_FMT_ZSTD && c->dctx) {
		ZSTD_freeDCtx(c->dctx);
	}
	if (c->file)
		fclose(c->file);
	free(c->in);
	free(c->out);
	free(c);
}

// work payload for a pull
typedef struct {
	nx_decompress_file_t *ctx;
	uint8_t *result;      // produced output for this pull (malloc'd, may be NULL)
	size_t result_size;
	bool done;            // true => stream finished, return null
	int err;
} decompress_file_pull_t;

// Refill the input buffer from the file if it is empty and there is more to
// read. Returns false on read error (sets *err).
static bool fused_refill(nx_decompress_file_t *c, int *err) {
	if (c->in_pos < c->in_size)
		return true; // still have buffered input
	if (c->eof_in)
		return true; // nothing more to read; caller handles drain/flush
	size_t want = NX_FUSED_IN_CHUNK;
	if (c->remaining < (uint64_t)want)
		want = (size_t)c->remaining;
	if (want == 0) {
		c->eof_in = true;
		return true;
	}
	size_t got = fread(c->in, 1, want, c->file);
	if (got != want && ferror(c->file)) {
		*err = errno ? errno : -1;
		return false;
	}
	c->in_size = got;
	c->in_pos = 0;
	c->remaining -= got;
	if (got < want || c->remaining == 0)
		c->eof_in = true;
	return true;
}

void decompress_file_pull_do(nx_work_t *req) {
	decompress_file_pull_t *d = (decompress_file_pull_t *)req->data;
	nx_decompress_file_t *c = d->ctx;
	if (c->finished) {
		d->done = true;
		return;
	}

	if (c->format == NX_FMT_ZSTD) {
		ZSTD_outBuffer output = {c->out, c->out_cap, 0};
		// Produce up to one OUT_CHUNK of output, reading/refilling input as
		// needed. Stop when the output buffer fills or input is exhausted.
		while (output.pos < output.size) {
			if (!fused_refill(c, &d->err))
				return;
			if (c->in_pos >= c->in_size) {
				// no more input available
				if (c->eof_in) {
					c->finished = true;
					break;
				}
				break;
			}
			ZSTD_inBuffer input = {c->in, c->in_size, c->in_pos};
			size_t ret = ZSTD_decompressStream(c->dctx, &output, &input);
			c->in_pos = input.pos;
			if (ZSTD_isError(ret)) {
				d->err = -4;
				return;
			}
			// ret==0 means a frame completed; with eof + all input consumed
			// we are done.
			if (c->eof_in && c->in_pos >= c->in_size && ret == 0) {
				c->finished = true;
				break;
			}
		}
		d->result_size = output.pos;
	} else {
		// zlib / gzip / deflate
		z_stream *s = c->zstream;
		s->next_out = c->out;
		s->avail_out = (uInt)c->out_cap;
		while (s->avail_out > 0) {
			if (!fused_refill(c, &d->err))
				return;
			s->next_in = c->in + c->in_pos;
			s->avail_in = (uInt)(c->in_size - c->in_pos);
			int ret = inflate(s, c->eof_in ? Z_SYNC_FLUSH : Z_NO_FLUSH);
			c->in_pos = c->in_size - s->avail_in;
			if (ret == Z_STREAM_END) {
				c->finished = true;
				break;
			}
			if (ret == Z_BUF_ERROR) {
				// no progress possible right now; if eof + no input, finish
				if (c->eof_in && c->in_pos >= c->in_size) {
					c->finished = true;
					break;
				}
				break;
			}
			if (ret != Z_OK) {
				d->err = ret;
				return;
			}
			if (c->in_pos >= c->in_size && c->eof_in)
				continue; // let inflate drain with Z_SYNC_FLUSH
		}
		d->result_size = c->out_cap - s->avail_out;
	}

	if (d->result_size > 0) {
		d->result = (uint8_t *)malloc(d->result_size);
		if (!d->result) {
			d->err = ENOMEM;
			return;
		}
		memcpy(d->result, c->out, d->result_size);
	} else if (c->finished) {
		d->done = true;
	}
}

MaybeLocal<Value> decompress_file_pull_cb(Isolate *iso, nx_work_t *req) {
	decompress_file_pull_t *d = (decompress_file_pull_t *)req->data;
	if (d->err) {
		if (d->result) {
			free(d->result);
			d->result = nullptr;
		}
		if (d->err == -4)
			nx_throw(iso, "zstd decompression error");
		else
			nx_throw_errno_error(iso, d->err, "decompressFilePull");
		return MaybeLocal<Value>();
	}
	if (d->done && d->result_size == 0) {
		return Null(iso).As<Value>(); // signal stream end
	}
	uint8_t *buf = d->result;
	size_t size = d->result_size;
	d->result = nullptr;
	std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
	    buf, size, [](void *p, size_t, void *) { free(p); }, nullptr);
	return ArrayBuffer::New(iso, std::move(bs)).As<Value>();
}

// $.decompressFileNew(format, path, start, end) -> handle
void nx_decompress_file_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value format(iso, info[0]);
	String::Utf8Value path(iso, info[1]);
	if (!*format || !*path) {
		nx_throw(iso, "decompressFileNew: format and path required");
		return;
	}
	fmt_t fmt = format_from_string(*format);
	if (fmt == NX_FMT_UNKNOWN) {
		nx_throw(iso, "Invalid compression format");
		return;
	}
	Local<Context> ctx = iso->GetCurrentContext();
	uint64_t start = 0, end = 0;
	bool have_end = false;
	if (info.Length() > 2 && info[2]->IsNumber())
		start = (uint64_t)info[2]->NumberValue(ctx).FromMaybe(0);
	if (info.Length() > 3 && info[3]->IsNumber()) {
		end = (uint64_t)info[3]->NumberValue(ctx).FromMaybe(0);
		have_end = true;
	}
	// Per-pull output capacity (arg 5), clamped. Larger => fewer thread-pool
	// dispatches per MB of output (the dominant cost), at the price of peak
	// memory (in + out + one result copy in flight). The caller picks this by
	// memory regime.
	size_t out_cap = 1024 * 1024;
	if (info.Length() > 4 && info[4]->IsNumber())
		out_cap = (size_t)info[4]->NumberValue(ctx).FromMaybe(out_cap);
	if (out_cap < NX_FUSED_OUT_MIN)
		out_cap = NX_FUSED_OUT_MIN;
	if (out_cap > NX_FUSED_OUT_MAX)
		out_cap = NX_FUSED_OUT_MAX;

	FILE *file = fopen(*path, "rb");
	if (!file) {
		nx_throw_errno_error(iso, errno, "fopen");
		return;
	}
	if (start && fseek(file, (long)start, SEEK_SET) != 0) {
		fclose(file);
		nx_throw_errno_error(iso, errno, "fseek");
		return;
	}
	uint64_t remaining = UINT64_MAX;
	if (have_end && end >= start)
		remaining = end - start;

	nx_decompress_file_t *c =
	    (nx_decompress_file_t *)calloc(1, sizeof(nx_decompress_file_t));
	c->format = fmt;
	c->file = file;
	c->remaining = remaining;
	c->out_cap = out_cap;
	c->in = (uint8_t *)malloc(NX_FUSED_IN_CHUNK);
	c->out = (uint8_t *)malloc(out_cap);
	if (!c->in || !c->out) {
		free_decompress_file(c);
		nx_throw(iso, "decompressFileNew: out of memory");
		return;
	}
	if (is_zlib(fmt)) {
		z_stream *s = (z_stream *)calloc(1, sizeof(z_stream));
		s->zalloc = Z_NULL;
		s->zfree = Z_NULL;
		s->opaque = Z_NULL;
		if (inflateInit2(s, zlib_window_bits(fmt)) != Z_OK) {
			free(s);
			free_decompress_file(c);
			nx_throw(iso, "Failed to initialize inflate stream");
			return;
		}
		c->zstream = s;
	} else {
		c->dctx = ZSTD_createDCtx();
		if (!c->dctx) {
			free_decompress_file(c);
			nx_throw(iso, "ZSTD_createDCtx() returned NULL");
			return;
		}
	}
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_decompress_file_t>(iso, obj, c, free_decompress_file);
	info.GetReturnValue().Set(obj);
}

// $.decompressFilePull(handle) -> Promise<ArrayBuffer | null>
void nx_decompress_file_pull(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_decompress_file_t *c = nx::Unwrap<nx_decompress_file_t>(info[0]);
	if (!c) {
		nx_throw(iso, "decompressFilePull: invalid handle");
		return;
	}
	nx_work_t *req = new nx_work_t();
	decompress_file_pull_t *data =
	    (decompress_file_pull_t *)calloc(1, sizeof(decompress_file_pull_t));
	data->ctx = c;
	req->data = data;
	info.GetReturnValue().Set(nx_queue_async(
	    iso, req, decompress_file_pull_do, decompress_file_pull_cb));
}

} // namespace

void nx_init_compression(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "compressNew", nx_compress_new);
	NX_SET_FUNC(init_obj, "compressWrite", nx_compress_write);
	NX_SET_FUNC(init_obj, "compressFlush", nx_compress_flush);
	NX_SET_FUNC(init_obj, "decompressNew", nx_decompress_new);
	NX_SET_FUNC(init_obj, "decompressWrite", nx_decompress_write);
	NX_SET_FUNC(init_obj, "decompressFlush", nx_decompress_flush);
	NX_SET_FUNC(init_obj, "decompressFileNew", nx_decompress_file_new);
	NX_SET_FUNC(init_obj, "decompressFilePull", nx_decompress_file_pull);
}
