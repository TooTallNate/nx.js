#pragma once
#include "types.h"
#include <zlib.h>
#include <zstd.h>

typedef enum {
	NX_COMPRESSION_FORMAT_UNKNOWN,
	NX_COMPRESSION_FORMAT_DEFLATE,
	NX_COMPRESSION_FORMAT_DEFLATE_RAW,
	NX_COMPRESSION_FORMAT_GZIP,
	NX_COMPRESSION_FORMAT_ZSTD,
} nx_compression_format_t;

typedef struct {
	nx_compression_format_t format;
	union {
		z_stream *zstream;
		ZSTD_CCtx *cctx;
	};
} nx_compress_t;

typedef struct {
	nx_compression_format_t format;
	union {
		z_stream *zstream;
		ZSTD_DCtx *dctx;
	};
} nx_decompress_t;

void nx_init_compression(JSContext *ctx, JSValueConst init_obj);
