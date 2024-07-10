#include "crypto.h"
#include "async.h"
#include "errno.h"
#include <switch.h>

typedef struct {
	int err;
	const char *algorithm;
	JSValue data_val;
	uint8_t *data;
	size_t size;
	uint8_t *result;
	size_t result_size;
} nx_crypto_digest_async_t;

enum nx_crypto_algorithm {
	NX_CRYPTO_SHA1,
	NX_CRYPTO_SHA256,
};

static void free_array_buffer(JSRuntime *rt, void *opaque, void *ptr) {
	free(ptr);
}

void nx_crypto_digest_do(nx_work_t *req) {
	nx_crypto_digest_async_t *data = (nx_crypto_digest_async_t *)req->data;
	enum nx_crypto_algorithm alg = -1;
	if (strcasecmp(data->algorithm, "SHA-1") == 0) {
		alg = NX_CRYPTO_SHA1;
		data->result_size = SHA1_HASH_SIZE;
	} else if (strcasecmp(data->algorithm, "SHA-256") == 0) {
		alg = NX_CRYPTO_SHA256;
		data->result_size = SHA256_HASH_SIZE;
	}
	if (alg == -1) {
		data->err = ENOTSUP;
		return;
	}
	data->result = calloc(1, data->result_size);
	if (!data->result) {
		data->err = ENOMEM;
		return;
	}
	switch (alg) {
	case NX_CRYPTO_SHA1:
		sha1CalculateHash(data->result, data->data, data->size);
		break;
	case NX_CRYPTO_SHA256:
		sha256CalculateHash(data->result, data->data, data->size);
		break;
	}
}

JSValue nx_crypto_digest_cb(JSContext *ctx, nx_work_t *req) {
	nx_crypto_digest_async_t *data = (nx_crypto_digest_async_t *)req->data;
	JS_FreeCString(ctx, data->algorithm);
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

static JSValue nx_crypto_digest(JSContext *ctx, JSValueConst this_val, int argc,
								JSValueConst *argv) {
	NX_INIT_WORK_T(nx_crypto_digest_async_t);
	data->algorithm = JS_ToCString(ctx, argv[0]);
	if (!data->algorithm) {
		return JS_EXCEPTION;
	}
	data->data_val = JS_DupValue(ctx, argv[1]);
	data->data = JS_GetArrayBuffer(ctx, &data->size, data->data_val);
	return nx_queue_async(ctx, req, nx_crypto_digest_do, nx_crypto_digest_cb);
}

static JSValue nx_crypto_random_bytes(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	size_t size;
	void *buf = JS_GetArrayBuffer(ctx, &size, argv[0]);

	int offset;
	int length;
	if (JS_ToInt32(ctx, &offset, argv[1]) ||
		JS_ToInt32(ctx, &length, argv[2]) || offset + length > size) {
		JS_ThrowTypeError(ctx, "invalid input");
		return JS_EXCEPTION;
	}
	randomGet(buf + offset, length);
	return JS_UNDEFINED;
}

static JSValue nx_crypto_sha256_hex(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	size_t size;
	const char *str = JS_ToCStringLen(ctx, &size, argv[0]);
	if (!str) {
		return JS_EXCEPTION;
	}
	u8 *digest = js_mallocz(ctx, SHA256_HASH_SIZE);
	if (!digest) {
		return JS_EXCEPTION;
	}
	sha256CalculateHash(digest, str, size);
	JS_FreeCString(ctx, str);
	char *hex = js_malloc(ctx, (SHA256_HASH_SIZE * 2) + 1);
	for (int i = 0; i < SHA256_HASH_SIZE; i++) {
		snprintf(hex + i * 2, 3, "%02x", digest[i]);
	}
	JSValue hex_val = JS_NewString(ctx, hex);
	js_free(ctx, hex);
	js_free(ctx, digest);
	return hex_val;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("cryptoDigest", 0, nx_crypto_digest),
	JS_CFUNC_DEF("cryptoRandomBytes", 0, nx_crypto_random_bytes),
	JS_CFUNC_DEF("sha256Hex", 0, nx_crypto_sha256_hex),
};

void nx_init_crypto(JSContext *ctx, JSValueConst init_obj) {
	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
