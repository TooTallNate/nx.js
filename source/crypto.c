#include "crypto.h"
#include "async.h"
#include "util.h"
#include <errno.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/hkdf.h>
#include <mbedtls/sha512.h>
#include <string.h>
#include <switch.h>

static JSClassID nx_crypto_key_class_id;

typedef struct {
	int err;
	const char *algorithm;
	JSValue data_val;
	uint8_t *data;
	size_t size;
	uint8_t *result;
	size_t result_size;
} nx_crypto_digest_async_t;

typedef struct {
	int err;

	JSValue algorithm_val;
	void *algorithm_params;

	JSValue key_val;
	nx_crypto_key_t *key;

	JSValue data_val;
	void *data;
	size_t data_size;

	void *result;
	size_t result_size;
} nx_crypto_encrypt_async_t;

typedef struct {
	u8 *iv;
} nx_crypto_aes_cbc_params_t;

typedef struct {
	u8 *ctr;
} nx_crypto_aes_ctr_params_t;

typedef struct {
	u64 sector;
	size_t sector_size;
	bool is_nintendo;
} nx_crypto_aes_xts_params_t;

enum nx_crypto_algorithm {
	NX_CRYPTO_SHA1,
	NX_CRYPTO_SHA256,
	NX_CRYPTO_SHA384,
	NX_CRYPTO_SHA512,
};

// Function to pad the input buffer to a multiple of `block_size`
// (PKCS#7 padding)
void *pad_pkcs7(size_t block_size, const uint8_t *input, size_t input_len,
				size_t *out_size) {
	size_t padded_len = ((input_len / block_size) + 1) *
						block_size; // Next multiple of `block_size`
	void *output = malloc(padded_len);
	if (output) {
		memcpy(output, input, input_len);                 // Copy original input
		uint8_t pad_value = padded_len - input_len;       // Padding value
		memset(output + input_len, pad_value, pad_value); // Add padding
		*out_size = padded_len;
	}
	return output;
}

// Function to remove PKCS#7 padding after decryption
size_t unpad_pkcs7(size_t block_size, uint8_t *input, size_t input_len) {
	uint8_t pad_value = input[input_len - 1];
	if (pad_value > block_size || pad_value == 0)
		return input_len;         // Invalid padding
	return input_len - pad_value; // Return unpadded length
}

static void finalizer_crypto_key(JSRuntime *rt, JSValue val) {
	nx_crypto_key_t *context = JS_GetOpaque(val, nx_crypto_key_class_id);
	if (context) {
		JS_FreeValueRT(rt, context->algorithm_cached);
		JS_FreeValueRT(rt, context->usages_cached);
		if (context->algorithm == NX_CRYPTO_KEY_ALGORITHM_HMAC) {
			nx_crypto_key_hmac_t *hmac = context->handle;
			if (hmac && hmac->key) {
				js_free_rt(rt, hmac->key);
			}
		}
		if (context->handle) {
			js_free_rt(rt, context->handle);
		}
		if (context->raw_key_data) {
			js_free_rt(rt, context->raw_key_data);
		}
		js_free_rt(rt, context);
	}
}

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
	} else if (strcasecmp(data->algorithm, "SHA-384") == 0) {
		alg = NX_CRYPTO_SHA384;
		data->result_size = 0x30;
	} else if (strcasecmp(data->algorithm, "SHA-512") == 0) {
		alg = NX_CRYPTO_SHA512;
		data->result_size = 0x40;
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
	case NX_CRYPTO_SHA384:
	case NX_CRYPTO_SHA512:
		mbedtls_sha512_context ctx;
		mbedtls_sha512_init(&ctx);
		mbedtls_sha512_starts(
			&ctx, alg == NX_CRYPTO_SHA384); // 0 for SHA-512, 1 for SHA-384
		mbedtls_sha512_update(&ctx, data->data, data->size);
		mbedtls_sha512_finish(&ctx, data->result);
		mbedtls_sha512_free(&ctx);
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
		js_free(ctx, data);
		return JS_EXCEPTION;
	}
	data->data = NX_GetBufferSource(ctx, &data->size, argv[1]);
	if (!data->data) {
		JS_FreeCString(ctx, data->algorithm);
		js_free(ctx, data);
		return JS_EXCEPTION;
	}
	data->data_val = JS_DupValue(ctx, argv[1]);
	return nx_queue_async(ctx, req, nx_crypto_digest_do, nx_crypto_digest_cb);
}

void nx_crypto_encrypt_do(nx_work_t *req) {
	nx_crypto_encrypt_async_t *data = (nx_crypto_encrypt_async_t *)req->data;

	if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_CBC) {
		nx_crypto_key_aes_t *aes = (nx_crypto_key_aes_t *)data->key->handle;
		nx_crypto_aes_cbc_params_t *cbc_params =
			(nx_crypto_aes_cbc_params_t *)data->algorithm_params;

		data->result = pad_pkcs7(AES_BLOCK_SIZE, data->data, data->data_size,
								 &data->result_size);
		if (!data->result) {
			data->err = ENOMEM;
			return;
		}

		if (aes->key_length == 16) {
			aes128CbcContextResetIv(&aes->encrypt.cbc_128, cbc_params->iv);
			aes128CbcEncrypt(&aes->encrypt.cbc_128, data->result, data->result,
							 data->result_size);
		} else if (aes->key_length == 24) {
			aes192CbcContextResetIv(&aes->encrypt.cbc_192, cbc_params->iv);
			aes192CbcEncrypt(&aes->encrypt.cbc_192, data->result, data->result,
							 data->result_size);
		} else if (aes->key_length == 32) {
			aes256CbcContextResetIv(&aes->encrypt.cbc_256, cbc_params->iv);
			aes256CbcEncrypt(&aes->encrypt.cbc_256, data->result, data->result,
							 data->result_size);
		}
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_CTR) {
		nx_crypto_key_aes_t *aes = (nx_crypto_key_aes_t *)data->key->handle;
		nx_crypto_aes_ctr_params_t *ctr_params =
			(nx_crypto_aes_ctr_params_t *)data->algorithm_params;

		data->result_size = data->data_size;
		data->result = malloc(data->result_size);
		if (!data->result) {
			data->err = ENOMEM;
			return;
		}

		if (aes->key_length == 16) {
			aes128CtrContextResetCtr(&aes->decrypt.ctr_128, ctr_params->ctr);
			aes128CtrCrypt(&aes->decrypt.ctr_128, data->result, data->data,
						   data->result_size);
		} else if (aes->key_length == 24) {
			aes192CtrContextResetCtr(&aes->decrypt.ctr_192, ctr_params->ctr);
			aes192CtrCrypt(&aes->decrypt.ctr_192, data->result, data->data,
						   data->result_size);
		} else if (aes->key_length == 32) {
			aes256CtrContextResetCtr(&aes->decrypt.ctr_256, ctr_params->ctr);
			aes256CtrCrypt(&aes->decrypt.ctr_256, data->result, data->data,
						   data->result_size);
		}
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_XTS) {
		nx_crypto_key_aes_t *aes = (nx_crypto_key_aes_t *)data->key->handle;
		nx_crypto_aes_xts_params_t *xts_params =
			(nx_crypto_aes_xts_params_t *)data->algorithm_params;

		// In XTS the encrypted size is exactly the plaintext size
		data->result = malloc(data->data_size);
		if (!data->result) {
			data->err = ENOMEM;
			return;
		}

		if (aes->key_length == 32) {
			void *dst = data->result;
			void *src = data->data;
			u64 sector = xts_params->sector;
			for (size_t i = 0; i < data->data_size;
				 i += xts_params->sector_size) {
				aes128XtsContextResetSector(&aes->encrypt.xts_128, sector++,
											xts_params->is_nintendo);
				data->result_size += aes128XtsEncrypt(
					&aes->encrypt.xts_128, dst, src, xts_params->sector_size);

				dst = (u8 *)dst + xts_params->sector_size;
				src = (u8 *)src + xts_params->sector_size;
			}
		} else if (aes->key_length == 48) {
			data->err = ENOTSUP;
		} else if (aes->key_length == 64) {
			data->err = ENOTSUP;
		}
	}
}

JSValue nx_crypto_encrypt_cb(JSContext *ctx, nx_work_t *req) {
	nx_crypto_encrypt_async_t *data = (nx_crypto_encrypt_async_t *)req->data;
	if (data->algorithm_params) {
		js_free(ctx, data->algorithm_params);
	}
	JS_FreeValue(ctx, data->algorithm_val);
	JS_FreeValue(ctx, data->key_val);
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

static JSValue nx_crypto_encrypt(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	NX_INIT_WORK_T(nx_crypto_encrypt_async_t);

	data->key = JS_GetOpaque2(ctx, argv[1], nx_crypto_key_class_id);
	if (!data->key) {
		js_free(ctx, data);
		return JS_EXCEPTION;
	}

	// Validate that the key may be used for encryption
	if (!(data->key->usages & NX_CRYPTO_KEY_USAGE_ENCRYPT)) {
		js_free(ctx, data);
		return JS_ThrowTypeError(
			ctx, "Key does not support the 'encrypt' operation");
	}

	data->data = NX_GetBufferSource(ctx, &data->data_size, argv[2]);
	if (!data->data) {
		js_free(ctx, data);
		return JS_EXCEPTION;
	}

	if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_CBC) {
		nx_crypto_aes_cbc_params_t *cbc_params =
			js_mallocz(ctx, sizeof(nx_crypto_aes_cbc_params_t));
		if (!cbc_params) {
			js_free(ctx, data);
			return JS_EXCEPTION;
		}

		size_t iv_size;
		cbc_params->iv = NX_GetBufferSource(
			ctx, &iv_size, JS_GetPropertyStr(ctx, argv[0], "iv"));
		if (!cbc_params->iv) {
			js_free(ctx, data);
			js_free(ctx, cbc_params);
			return JS_EXCEPTION;
		}

		// Validate IV size
		if (iv_size != 16) {
			js_free(ctx, data);
			js_free(ctx, cbc_params);
			return JS_ThrowTypeError(
				ctx, "Initialization vector must be 16 bytes (got %lu)",
				iv_size);
		}

		data->algorithm_params = cbc_params;
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_CTR) {
		nx_crypto_aes_ctr_params_t *ctr_params =
			js_mallocz(ctx, sizeof(nx_crypto_aes_ctr_params_t));
		if (!ctr_params) {
			js_free(ctx, data);
			return JS_EXCEPTION;
		}

		size_t ctr_size;
		ctr_params->ctr = NX_GetBufferSource(
			ctx, &ctr_size, JS_GetPropertyStr(ctx, argv[0], "counter"));
		if (!ctr_params->ctr) {
			js_free(ctx, data);
			js_free(ctx, ctr_params);
			return JS_EXCEPTION;
		}

		// Validate counter size
		if (ctr_size != 16) {
			js_free(ctx, data);
			js_free(ctx, ctr_params);
			return JS_ThrowTypeError(ctx, "Counter must be 16 bytes (got %lu)",
									 ctr_size);
		}

		data->algorithm_params = ctr_params;
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_XTS) {
		nx_crypto_aes_xts_params_t *xts_params =
			js_mallocz(ctx, sizeof(nx_crypto_aes_xts_params_t));
		if (!xts_params) {
			js_free(ctx, data);
			return JS_EXCEPTION;
		}

		int is_nintendo =
			JS_ToBool(ctx, JS_GetPropertyStr(ctx, argv[0], "isNintendo"));

		u32 sector;
		u32 sector_size;
		if (is_nintendo == -1 ||
			JS_ToUint32(ctx, &sector,
						JS_GetPropertyStr(ctx, argv[0], "sector")) ||
			JS_ToUint32(ctx, &sector_size,
						JS_GetPropertyStr(ctx, argv[0], "sectorSize"))) {
			js_free(ctx, data);
			js_free(ctx, xts_params);
			return JS_EXCEPTION;
		}
		xts_params->is_nintendo = is_nintendo;
		xts_params->sector = sector;
		xts_params->sector_size = sector_size;

		data->algorithm_params = xts_params;
	}

	data->algorithm_val = JS_DupValue(ctx, argv[0]);
	data->key_val = JS_DupValue(ctx, argv[1]);
	data->data_val = JS_DupValue(ctx, argv[2]);

	return nx_queue_async(ctx, req, nx_crypto_encrypt_do, nx_crypto_encrypt_cb);
}

static JSValue nx_crypto_get_random_values(JSContext *ctx,
										   JSValueConst this_val, int argc,
										   JSValueConst *argv) {
	if (argc < 1) {
		return JS_ThrowTypeError(
			ctx,
			"Failed to execute 'getRandomValues' on 'Crypto': "
			"1 argument required, but only %d present",
			argc);
	}
	size_t size;
	void *buf = NX_GetBufferSource(ctx, &size, argv[0]);
	if (buf) {
		randomGet(buf, size);
	}
	return JS_DupValue(ctx, argv[0]);
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
	char hex[SHA256_HASH_SIZE * 2 + 1];
	for (int i = 0; i < SHA256_HASH_SIZE; i++) {
		snprintf(hex + i * 2, 3, "%02x", digest[i]);
	}
	JSValue hex_val = JS_NewString(ctx, hex);
	js_free(ctx, digest);
	return hex_val;
}

static JSValue nx_crypto_key_get_type(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_crypto_key_t *context =
		JS_GetOpaque2(ctx, this_val, nx_crypto_key_class_id);
	if (!context)
		return JS_EXCEPTION;

	char *type;
	switch (context->type) {
	case NX_CRYPTO_KEY_TYPE_UNKNOWN:
		type = "unknown";
		break;
	case NX_CRYPTO_KEY_TYPE_PRIVATE:
		type = "private";
		break;
	case NX_CRYPTO_KEY_TYPE_PUBLIC:
		type = "public";
		break;
	case NX_CRYPTO_KEY_TYPE_SECRET:
		type = "secret";
		break;
	}
	return JS_NewString(ctx, type);
}

static JSValue nx_crypto_key_get_extractable(JSContext *ctx,
											 JSValueConst this_val, int argc,
											 JSValueConst *argv) {
	nx_crypto_key_t *context =
		JS_GetOpaque2(ctx, this_val, nx_crypto_key_class_id);
	if (!context)
		return JS_EXCEPTION;

	return JS_NewBool(ctx, context->extractable);
}

static JSValue nx_crypto_key_get_algorithm(JSContext *ctx,
										   JSValueConst this_val, int argc,
										   JSValueConst *argv) {
	nx_crypto_key_t *context =
		JS_GetOpaque2(ctx, this_val, nx_crypto_key_class_id);
	if (!context)
		return JS_EXCEPTION;

	if (JS_IsUndefined(context->algorithm_cached)) {
		JSValue obj = JS_NewObject(ctx);

		char *name_val = "";
		switch (context->algorithm) {
		case NX_CRYPTO_KEY_ALGORITHM_AES_CBC:
			name_val = "AES-CBC";
			break;
		case NX_CRYPTO_KEY_ALGORITHM_AES_CTR:
			name_val = "AES-CTR";
			break;
		case NX_CRYPTO_KEY_ALGORITHM_AES_XTS:
			name_val = "AES-XTS";
			break;
		case NX_CRYPTO_KEY_ALGORITHM_PBKDF2:
			name_val = "PBKDF2";
			break;
		case NX_CRYPTO_KEY_ALGORITHM_HKDF:
			name_val = "HKDF";
			break;
		case NX_CRYPTO_KEY_ALGORITHM_HMAC: {
			name_val = "HMAC";
			nx_crypto_key_hmac_t *hmac = (nx_crypto_key_hmac_t *)context->handle;
			JSValue hash_obj = JS_NewObject(ctx);
			JS_SetPropertyStr(ctx, hash_obj, "name",
							  JS_NewString(ctx, hmac->hash_name));
			JS_SetPropertyStr(ctx, obj, "hash", hash_obj);
			JS_SetPropertyStr(ctx, obj, "length",
							  JS_NewUint32(ctx, hmac->key_length * 8));
			break;
		}
		default:
			// TODO: throw error?
			break;
		}
		JS_SetPropertyStr(ctx, obj, "name", JS_NewString(ctx, name_val));

		if (context->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_CBC ||
			context->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_CTR ||
			context->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_XTS) {
			nx_crypto_key_aes_t *aes = (nx_crypto_key_aes_t *)context->handle;
			JS_SetPropertyStr(ctx, obj, "length",
							  JS_NewUint32(ctx, aes->key_length * 8));
		}
		context->algorithm_cached = obj;
	}

	return JS_DupValue(ctx, context->algorithm_cached);
}

static JSValue nx_crypto_key_get_usages(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	nx_crypto_key_t *context =
		JS_GetOpaque2(ctx, this_val, nx_crypto_key_class_id);
	if (!context)
		return JS_EXCEPTION;

	if (JS_IsUndefined(context->usages_cached)) {
		size_t index = 0;
		JSValue arr = JS_NewArray(ctx);

		if (context->usages & NX_CRYPTO_KEY_USAGE_DECRYPT) {
			JS_SetPropertyUint32(ctx, arr, index++,
								 JS_NewString(ctx, "decrypt"));
		}
		if (context->usages & NX_CRYPTO_KEY_USAGE_DERIVE_BITS) {
			JS_SetPropertyUint32(ctx, arr, index++,
								 JS_NewString(ctx, "deriveBits"));
		}
		if (context->usages & NX_CRYPTO_KEY_USAGE_DERIVE_KEY) {
			JS_SetPropertyUint32(ctx, arr, index++,
								 JS_NewString(ctx, "deriveKey"));
		}
		if (context->usages & NX_CRYPTO_KEY_USAGE_ENCRYPT) {
			JS_SetPropertyUint32(ctx, arr, index++,
								 JS_NewString(ctx, "encrypt"));
		}
		if (context->usages & NX_CRYPTO_KEY_USAGE_SIGN) {
			JS_SetPropertyUint32(ctx, arr, index++, JS_NewString(ctx, "sign"));
		}
		if (context->usages & NX_CRYPTO_KEY_USAGE_UNWRAP_KEY) {
			JS_SetPropertyUint32(ctx, arr, index++,
								 JS_NewString(ctx, "unwrapKey"));
		}
		if (context->usages & NX_CRYPTO_KEY_USAGE_VERIFY) {
			JS_SetPropertyUint32(ctx, arr, index++,
								 JS_NewString(ctx, "verify"));
		}
		if (context->usages & NX_CRYPTO_KEY_USAGE_WRAP_KEY) {
			JS_SetPropertyUint32(ctx, arr, index++,
								 JS_NewString(ctx, "wrapKey"));
		}

		context->usages_cached = arr;
	}

	return JS_DupValue(ctx, context->usages_cached);
}

static JSValue nx_crypto_key_new(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	nx_crypto_key_t *context = js_mallocz(ctx, sizeof(nx_crypto_key_t));
	if (!context)
		return JS_EXCEPTION;

	context->algorithm_cached = JS_UNDEFINED;
	context->usages_cached = JS_UNDEFINED;

	size_t key_size;
	const void *key_data = NX_GetBufferSource(ctx, &key_size, argv[1]);
	if (!key_data) {
		js_free(ctx, context);
		return JS_EXCEPTION;
	}

	int extractable = JS_ToBool(ctx, argv[2]);
	if (extractable == -1) {
		js_free(ctx, context);
		return JS_EXCEPTION;
	}
	context->extractable = extractable;

	uint32_t usages_size;
	if (JS_ToUint32(ctx, &usages_size,
					JS_GetPropertyStr(ctx, argv[3], "length"))) {
		js_free(ctx, context);
		return JS_EXCEPTION;
	}
	for (uint32_t i = 0; i < usages_size; i++) {
		JSValue usage_val = JS_GetPropertyUint32(ctx, argv[3], i);
		if (!JS_IsString(usage_val)) {
			js_free(ctx, context);
			JS_ThrowTypeError(ctx, "Expected string for usage");
			return JS_EXCEPTION;
		}
		const char *usage = JS_ToCString(ctx, usage_val);
		if (!usage) {
			js_free(ctx, context);
			return JS_EXCEPTION;
		}
		if (strcmp(usage, "decrypt") == 0) {
			context->usages |= NX_CRYPTO_KEY_USAGE_DECRYPT;
		} else if (strcmp(usage, "deriveBits") == 0) {
			context->usages |= NX_CRYPTO_KEY_USAGE_DERIVE_BITS;
		} else if (strcmp(usage, "deriveKey") == 0) {
			context->usages |= NX_CRYPTO_KEY_USAGE_DERIVE_KEY;
		} else if (strcmp(usage, "encrypt") == 0) {
			context->usages |= NX_CRYPTO_KEY_USAGE_ENCRYPT;
		} else if (strcmp(usage, "sign") == 0) {
			context->usages |= NX_CRYPTO_KEY_USAGE_SIGN;
		} else if (strcmp(usage, "unwrapKey") == 0) {
			context->usages |= NX_CRYPTO_KEY_USAGE_UNWRAP_KEY;
		} else if (strcmp(usage, "verify") == 0) {
			context->usages |= NX_CRYPTO_KEY_USAGE_VERIFY;
		} else if (strcmp(usage, "wrapKey") == 0) {
			context->usages |= NX_CRYPTO_KEY_USAGE_WRAP_KEY;
		} else {
			js_free(ctx, context);
			JS_FreeCString(ctx, usage);
			JS_ThrowTypeError(ctx, "Invalid usage");
			return JS_EXCEPTION;
		}
		JS_FreeCString(ctx, usage);
	}

	JSValue algo_val = JS_GetPropertyStr(ctx, argv[0], "name");
	if (!JS_IsString(algo_val)) {
		js_free(ctx, context);
		JS_ThrowTypeError(ctx, "Expected string for algorithm \"name\"");
		return JS_EXCEPTION;
	}
	const char *algo = JS_ToCString(ctx, algo_val);
	if (!algo) {
		js_free(ctx, context);
		return JS_EXCEPTION;
	}

	// Store raw key data for exportKey support
	context->raw_key_data = js_malloc(ctx, key_size);
	if (!context->raw_key_data) {
		js_free(ctx, context);
		JS_FreeValue(ctx, algo_val);
		JS_FreeCString(ctx, algo);
		return JS_EXCEPTION;
	}
	memcpy(context->raw_key_data, key_data, key_size);
	context->raw_key_size = key_size;

	if (strcmp(algo, "AES-CBC") == 0) {
		if (key_size != 16 && key_size != 24 && key_size != 32) {
			js_free(ctx, context->raw_key_data);
			js_free(ctx, context);
			JS_FreeCString(ctx, algo);
			JS_FreeValue(ctx, algo_val);
			JS_ThrowPlainError(ctx, "Invalid key length");
			return JS_EXCEPTION;
		}
		context->type = NX_CRYPTO_KEY_TYPE_SECRET;
		context->algorithm = NX_CRYPTO_KEY_ALGORITHM_AES_CBC;

		nx_crypto_key_aes_t *aes = js_mallocz(ctx, sizeof(nx_crypto_key_aes_t));
		if (!aes) {
			js_free(ctx, context->raw_key_data);
			js_free(ctx, context);
			JS_FreeCString(ctx, algo);
			JS_FreeValue(ctx, algo_val);
			return JS_EXCEPTION;
		}
		context->handle = aes;
		aes->key_length = key_size;

		if (key_size == 16) {
			if (context->usages & NX_CRYPTO_KEY_USAGE_ENCRYPT) {
				aes128CbcContextCreate(&aes->encrypt.cbc_128, key_data,
									   key_data, true);
			}
			if (context->usages & NX_CRYPTO_KEY_USAGE_DECRYPT) {
				aes128CbcContextCreate(&aes->decrypt.cbc_128, key_data,
									   key_data, false);
			}
		} else if (key_size == 24) {
			if (context->usages & NX_CRYPTO_KEY_USAGE_ENCRYPT) {
				aes192CbcContextCreate(&aes->encrypt.cbc_192, key_data,
									   key_data, true);
			}
			if (context->usages & NX_CRYPTO_KEY_USAGE_DECRYPT) {
				aes192CbcContextCreate(&aes->decrypt.cbc_192, key_data,
									   key_data, false);
			}
		} else {
			if (context->usages & NX_CRYPTO_KEY_USAGE_ENCRYPT) {
				aes256CbcContextCreate(&aes->encrypt.cbc_256, key_data,
									   key_data, true);
			}
			if (context->usages & NX_CRYPTO_KEY_USAGE_DECRYPT) {
				aes256CbcContextCreate(&aes->decrypt.cbc_256, key_data,
									   key_data, false);
			}
		}
	} else if (strcmp(algo, "AES-CTR") == 0) {
		if (key_size != 16 && key_size != 24 && key_size != 32) {
			js_free(ctx, context->raw_key_data);
			js_free(ctx, context);
			JS_FreeCString(ctx, algo);
			JS_FreeValue(ctx, algo_val);
			JS_ThrowPlainError(ctx, "Invalid key length");
			return JS_EXCEPTION;
		}
		context->type = NX_CRYPTO_KEY_TYPE_SECRET;
		context->algorithm = NX_CRYPTO_KEY_ALGORITHM_AES_CTR;

		nx_crypto_key_aes_t *aes = js_mallocz(ctx, sizeof(nx_crypto_key_aes_t));
		if (!aes) {
			js_free(ctx, context->raw_key_data);
			js_free(ctx, context);
			JS_FreeCString(ctx, algo);
			JS_FreeValue(ctx, algo_val);
			return JS_EXCEPTION;
		}
		context->handle = aes;
		aes->key_length = key_size;

		if (key_size == 16) {
			if (context->usages & NX_CRYPTO_KEY_USAGE_ENCRYPT ||
				context->usages & NX_CRYPTO_KEY_USAGE_DECRYPT) {
				aes128CtrContextCreate(&aes->decrypt.ctr_128, key_data,
									   key_data);
			}
		} else if (key_size == 24) {
			if (context->usages & NX_CRYPTO_KEY_USAGE_ENCRYPT ||
				context->usages & NX_CRYPTO_KEY_USAGE_DECRYPT) {
				aes192CtrContextCreate(&aes->decrypt.ctr_192, key_data,
									   key_data);
			}
		} else {
			if (context->usages & NX_CRYPTO_KEY_USAGE_ENCRYPT ||
				context->usages & NX_CRYPTO_KEY_USAGE_DECRYPT) {
				aes256CtrContextCreate(&aes->decrypt.ctr_256, key_data,
									   key_data);
			}
		}
	} else if (strcmp(algo, "AES-XTS") == 0) {
		if (key_size != 32 && key_size != 48 && key_size != 64) {
			js_free(ctx, context->raw_key_data);
			js_free(ctx, context);
			JS_FreeCString(ctx, algo);
			JS_FreeValue(ctx, algo_val);
			JS_ThrowPlainError(ctx, "Invalid key length");
			return JS_EXCEPTION;
		}
		context->type = NX_CRYPTO_KEY_TYPE_SECRET;
		context->algorithm = NX_CRYPTO_KEY_ALGORITHM_AES_XTS;

		nx_crypto_key_aes_t *aes = js_mallocz(ctx, sizeof(nx_crypto_key_aes_t));
		if (!aes) {
			js_free(ctx, context->raw_key_data);
			js_free(ctx, context);
			JS_FreeCString(ctx, algo);
			JS_FreeValue(ctx, algo_val);
			return JS_EXCEPTION;
		}
		context->handle = aes;
		aes->key_length = key_size;

		if (key_size == 32) {
			if (context->usages & NX_CRYPTO_KEY_USAGE_ENCRYPT) {
				aes128XtsContextCreate(&aes->encrypt.xts_128, key_data,
									   key_data + 0x10, true);
			}
			if (context->usages & NX_CRYPTO_KEY_USAGE_DECRYPT) {
				aes128XtsContextCreate(&aes->decrypt.xts_128, key_data,
									   key_data + 0x10, false);
			}
		} else if (key_size == 48) {
			if (context->usages & NX_CRYPTO_KEY_USAGE_ENCRYPT) {
				aes192XtsContextCreate(&aes->encrypt.xts_192, key_data,
									   key_data + 0x18, true);
			}
			if (context->usages & NX_CRYPTO_KEY_USAGE_DECRYPT) {
				aes192XtsContextCreate(&aes->decrypt.xts_192, key_data,
									   key_data + 0x18, false);
			}
		} else {
			if (context->usages & NX_CRYPTO_KEY_USAGE_ENCRYPT) {
				aes256XtsContextCreate(&aes->encrypt.xts_256, key_data,
									   key_data + 0x20, true);
			}
			if (context->usages & NX_CRYPTO_KEY_USAGE_DECRYPT) {
				aes256XtsContextCreate(&aes->decrypt.xts_256, key_data,
									   key_data + 0x20, false);
			}
		}
	} else if (strcmp(algo, "PBKDF2") == 0) {
		context->type = NX_CRYPTO_KEY_TYPE_SECRET;
		context->algorithm = NX_CRYPTO_KEY_ALGORITHM_PBKDF2;
		// PBKDF2 keys don't need a handle - raw key data is sufficient
		context->handle = NULL;
	} else if (strcmp(algo, "HKDF") == 0) {
		context->type = NX_CRYPTO_KEY_TYPE_SECRET;
		context->algorithm = NX_CRYPTO_KEY_ALGORITHM_HKDF;
		// HKDF keys don't need a handle - raw key data is sufficient
		context->handle = NULL;
	} else if (strcmp(algo, "HMAC") == 0) {
		context->type = NX_CRYPTO_KEY_TYPE_SECRET;
		context->algorithm = NX_CRYPTO_KEY_ALGORITHM_HMAC;

		// Get the hash algorithm from the algorithm object
		JSValue hash_val = JS_GetPropertyStr(ctx, argv[0], "hash");
		const char *hash_name;
		JSValue hash_name_val = JS_UNDEFINED;
		if (JS_IsString(hash_val)) {
			hash_name = JS_ToCString(ctx, hash_val);
		} else {
			hash_name_val = JS_GetPropertyStr(ctx, hash_val, "name");
			hash_name = JS_ToCString(ctx, hash_name_val);
			JS_FreeValue(ctx, hash_name_val);
		}
		JS_FreeValue(ctx, hash_val);

		if (!hash_name) {
			js_free(ctx, context->raw_key_data);
			js_free(ctx, context);
			JS_FreeValue(ctx, algo_val);
			JS_FreeCString(ctx, algo);
			return JS_EXCEPTION;
		}

		nx_crypto_key_hmac_t *hmac = js_mallocz(ctx, sizeof(nx_crypto_key_hmac_t));
		if (!hmac) {
			JS_FreeCString(ctx, hash_name);
			js_free(ctx, context->raw_key_data);
			js_free(ctx, context);
			JS_FreeValue(ctx, algo_val);
			JS_FreeCString(ctx, algo);
			return JS_EXCEPTION;
		}
		hmac->key = js_malloc(ctx, key_size);
		if (!hmac->key) {
			JS_FreeCString(ctx, hash_name);
			js_free(ctx, hmac);
			js_free(ctx, context->raw_key_data);
			js_free(ctx, context);
			JS_FreeValue(ctx, algo_val);
			JS_FreeCString(ctx, algo);
			return JS_EXCEPTION;
		}
		memcpy(hmac->key, key_data, key_size);
		hmac->key_length = key_size;
		strncpy(hmac->hash_name, hash_name, sizeof(hmac->hash_name) - 1);
		JS_FreeCString(ctx, hash_name);

		context->handle = hmac;
	} else {
		JS_ThrowTypeError(ctx, "Unrecognized algorithm name: \"%s\"", algo);
		js_free(ctx, context->raw_key_data);
		js_free(ctx, context);
		JS_FreeValue(ctx, algo_val);
		JS_FreeCString(ctx, algo);
		return JS_EXCEPTION;
	}

	JS_FreeValue(ctx, algo_val);
	JS_FreeCString(ctx, algo);

	JSValue obj = JS_NewObjectClass(ctx, nx_crypto_key_class_id);
	if (JS_IsException(obj)) {
		if (context->algorithm == NX_CRYPTO_KEY_ALGORITHM_HMAC) {
			nx_crypto_key_hmac_t *hmac = context->handle;
			if (hmac && hmac->key) {
				js_free(ctx, hmac->key);
			}
		}
		if (context->handle) {
			js_free(ctx, context->handle);
		}
		if (context->raw_key_data) {
			js_free(ctx, context->raw_key_data);
		}
		js_free(ctx, context);
		return obj;
	}

	JS_SetOpaque(obj, context);
	return obj;
}

static JSValue nx_crypto_key_init(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_GET(proto, "type", nx_crypto_key_get_type);
	NX_DEF_GET(proto, "extractable", nx_crypto_key_get_extractable);
	NX_DEF_GET(proto, "algorithm", nx_crypto_key_get_algorithm);
	NX_DEF_GET(proto, "usages", nx_crypto_key_get_usages);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

void nx_crypto_decrypt_do(nx_work_t *req) {
	nx_crypto_encrypt_async_t *data = (nx_crypto_encrypt_async_t *)req->data;

	if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_CBC) {
		nx_crypto_key_aes_t *aes = (nx_crypto_key_aes_t *)data->key->handle;
		nx_crypto_aes_cbc_params_t *cbc_params =
			(nx_crypto_aes_cbc_params_t *)data->algorithm_params;

		data->result = calloc(data->data_size, 1);
		if (!data->result) {
			data->err = ENOMEM;
			return;
		}

		if (aes->key_length == 16) {
			aes128CbcContextResetIv(&aes->decrypt.cbc_128, cbc_params->iv);
			aes128CbcDecrypt(&aes->decrypt.cbc_128, data->result, data->data,
							 data->data_size);
		} else if (aes->key_length == 24) {
			aes192CbcContextResetIv(&aes->decrypt.cbc_192, cbc_params->iv);
			aes192CbcDecrypt(&aes->decrypt.cbc_192, data->result, data->data,
							 data->data_size);
		} else if (aes->key_length == 32) {
			aes256CbcContextResetIv(&aes->decrypt.cbc_256, cbc_params->iv);
			aes256CbcDecrypt(&aes->decrypt.cbc_256, data->result, data->data,
							 data->data_size);
		}

		data->result_size =
			unpad_pkcs7(AES_BLOCK_SIZE, data->result, data->data_size);
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_CTR) {
		nx_crypto_key_aes_t *aes = (nx_crypto_key_aes_t *)data->key->handle;
		nx_crypto_aes_ctr_params_t *ctr_params =
			(nx_crypto_aes_ctr_params_t *)data->algorithm_params;

		data->result_size = data->data_size;
		data->result = malloc(data->result_size);
		if (!data->result) {
			data->err = ENOMEM;
			return;
		}

		if (aes->key_length == 16) {
			aes128CtrContextResetCtr(&aes->decrypt.ctr_128, ctr_params->ctr);
			aes128CtrCrypt(&aes->decrypt.ctr_128, data->result, data->data,
						   data->result_size);
		} else if (aes->key_length == 24) {
			aes192CtrContextResetCtr(&aes->decrypt.ctr_192, ctr_params->ctr);
			aes192CtrCrypt(&aes->decrypt.ctr_192, data->result, data->data,
						   data->result_size);
		} else if (aes->key_length == 32) {
			aes256CtrContextResetCtr(&aes->decrypt.ctr_256, ctr_params->ctr);
			aes256CtrCrypt(&aes->decrypt.ctr_256, data->result, data->data,
						   data->result_size);
		}
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_XTS) {
		nx_crypto_key_aes_t *aes = (nx_crypto_key_aes_t *)data->key->handle;
		nx_crypto_aes_xts_params_t *xts_params =
			(nx_crypto_aes_xts_params_t *)data->algorithm_params;

		// In XTS the decrypted size is exactly the ciphertext size
		data->result = malloc(data->data_size);
		if (!data->result) {
			data->err = ENOMEM;
			return;
		}

		if (aes->key_length == 32) {
			void *dst = data->result;
			void *src = data->data;
			uint64_t sector = xts_params->sector;
			for (size_t i = 0; i < data->data_size;
				 i += xts_params->sector_size) {
				aes128XtsContextResetSector(&aes->decrypt.xts_128, sector++,
											xts_params->is_nintendo);
				data->result_size += aes128XtsDecrypt(
					&aes->decrypt.xts_128, dst, src, xts_params->sector_size);

				dst = (u8 *)dst + xts_params->sector_size;
				src = (u8 *)src + xts_params->sector_size;
			}
		} else if (aes->key_length == 48) {
			data->err = ENOTSUP;
		} else if (aes->key_length == 64) {
			data->err = ENOTSUP;
		}
	}
}

JSValue nx_crypto_decrypt_cb(JSContext *ctx, nx_work_t *req) {
	nx_crypto_encrypt_async_t *data = (nx_crypto_encrypt_async_t *)req->data;
	if (data->algorithm_params) {
		js_free(ctx, data->algorithm_params);
	}
	JS_FreeValue(ctx, data->algorithm_val);
	JS_FreeValue(ctx, data->key_val);
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

static JSValue nx_crypto_subtle_decrypt(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	NX_INIT_WORK_T(nx_crypto_encrypt_async_t);

	data->key = JS_GetOpaque2(ctx, argv[1], nx_crypto_key_class_id);
	if (!data->key) {
		js_free(ctx, data);
		return JS_EXCEPTION;
	}

	// Validate that the key may be used for decryption
	if (!(data->key->usages & NX_CRYPTO_KEY_USAGE_DECRYPT)) {
		js_free(ctx, data);
		return JS_ThrowTypeError(
			ctx, "Key does not support the 'decrypt' operation");
	}

	data->data = NX_GetBufferSource(ctx, &data->data_size, argv[2]);
	if (!data->data) {
		js_free(ctx, data);
		return JS_EXCEPTION;
	}

	if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_CBC) {
		nx_crypto_aes_cbc_params_t *cbc_params =
			js_mallocz(ctx, sizeof(nx_crypto_aes_cbc_params_t));
		if (!cbc_params) {
			js_free(ctx, data);
			return JS_EXCEPTION;
		}

		size_t iv_size;
		cbc_params->iv = NX_GetBufferSource(
			ctx, &iv_size, JS_GetPropertyStr(ctx, argv[0], "iv"));
		if (!cbc_params->iv) {
			js_free(ctx, data);
			js_free(ctx, cbc_params);
			return JS_EXCEPTION;
		}

		// Validate IV size
		if (iv_size != 16) {
			js_free(ctx, data);
			js_free(ctx, cbc_params);
			return JS_ThrowTypeError(ctx,
									 "Initialization vector must be 16 bytes");
		}

		data->algorithm_params = cbc_params;
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_CTR) {
		nx_crypto_aes_ctr_params_t *ctr_params =
			js_mallocz(ctx, sizeof(nx_crypto_aes_ctr_params_t));
		if (!ctr_params) {
			js_free(ctx, data);
			return JS_EXCEPTION;
		}

		size_t ctr_size;
		ctr_params->ctr = NX_GetBufferSource(
			ctx, &ctr_size, JS_GetPropertyStr(ctx, argv[0], "counter"));
		if (!ctr_params->ctr) {
			js_free(ctx, data);
			js_free(ctx, ctr_params);
			return JS_EXCEPTION;
		}

		// Validate counter size
		if (ctr_size != 16) {
			js_free(ctx, data);
			js_free(ctx, ctr_params);
			return JS_ThrowTypeError(ctx, "Counter must be 16 bytes (got %lu)",
									 ctr_size);
		}

		data->algorithm_params = ctr_params;
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_XTS) {
		nx_crypto_aes_xts_params_t *xts_params =
			js_mallocz(ctx, sizeof(nx_crypto_aes_xts_params_t));
		if (!xts_params) {
			js_free(ctx, data);
			return JS_EXCEPTION;
		}

		int is_nintendo =
			JS_ToBool(ctx, JS_GetPropertyStr(ctx, argv[0], "isNintendo"));

		u32 sector;
		u32 sector_size;
		if (is_nintendo == -1 ||
			JS_ToUint32(ctx, &sector,
						JS_GetPropertyStr(ctx, argv[0], "sector")) ||
			JS_ToUint32(ctx, &sector_size,
						JS_GetPropertyStr(ctx, argv[0], "sectorSize"))) {
			js_free(ctx, data);
			js_free(ctx, xts_params);
			return JS_EXCEPTION;
		}
		xts_params->is_nintendo = is_nintendo;
		xts_params->sector = sector;
		xts_params->sector_size = sector_size;

		data->algorithm_params = xts_params;
	}

	data->algorithm_val = JS_DupValue(ctx, argv[0]);
	data->key_val = JS_DupValue(ctx, argv[1]);
	data->data_val = JS_DupValue(ctx, argv[2]);

	return nx_queue_async(ctx, req, nx_crypto_decrypt_do, nx_crypto_decrypt_cb);
}

// --- HMAC Sign ---

typedef struct {
	int err;
	JSValue algorithm_val;
	JSValue key_val;
	nx_crypto_key_t *key;
	JSValue data_val;
	void *data;
	size_t data_size;
	void *result;
	size_t result_size;
} nx_crypto_sign_async_t;

static mbedtls_md_type_t nx_crypto_get_md_type(const char *hash_name) {
	if (strcasecmp(hash_name, "SHA-1") == 0)
		return MBEDTLS_MD_SHA1;
	else if (strcasecmp(hash_name, "SHA-256") == 0)
		return MBEDTLS_MD_SHA256;
	else if (strcasecmp(hash_name, "SHA-384") == 0)
		return MBEDTLS_MD_SHA384;
	else if (strcasecmp(hash_name, "SHA-512") == 0)
		return MBEDTLS_MD_SHA512;
	return MBEDTLS_MD_NONE;
}

void nx_crypto_sign_do(nx_work_t *req) {
	nx_crypto_sign_async_t *data = (nx_crypto_sign_async_t *)req->data;

	if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_HMAC) {
		nx_crypto_key_hmac_t *hmac = (nx_crypto_key_hmac_t *)data->key->handle;

		mbedtls_md_type_t md_type = nx_crypto_get_md_type(hmac->hash_name);
		if (md_type == MBEDTLS_MD_NONE) {
			data->err = ENOTSUP;
			return;
		}

		const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(md_type);
		data->result_size = mbedtls_md_get_size(md_info);
		data->result = calloc(1, data->result_size);
		if (!data->result) {
			data->err = ENOMEM;
			return;
		}

		int ret = mbedtls_md_hmac(md_info, hmac->key, hmac->key_length,
								  data->data, data->data_size, data->result);
		if (ret != 0) {
			free(data->result);
			data->result = NULL;
			data->err = ret;
		}
	} else {
		data->err = ENOTSUP;
	}
}

JSValue nx_crypto_sign_cb(JSContext *ctx, nx_work_t *req) {
	nx_crypto_sign_async_t *data = (nx_crypto_sign_async_t *)req->data;
	JS_FreeValue(ctx, data->algorithm_val);
	JS_FreeValue(ctx, data->key_val);
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

static JSValue nx_crypto_sign(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	NX_INIT_WORK_T(nx_crypto_sign_async_t);

	data->key = JS_GetOpaque2(ctx, argv[1], nx_crypto_key_class_id);
	if (!data->key) {
		js_free(ctx, data);
		free(req);
		return JS_EXCEPTION;
	}

	if (!(data->key->usages & NX_CRYPTO_KEY_USAGE_SIGN)) {
		js_free(ctx, data);
		free(req);
		return JS_ThrowTypeError(ctx,
								 "Key does not support the 'sign' operation");
	}

	data->data = NX_GetBufferSource(ctx, &data->data_size, argv[2]);
	if (!data->data) {
		js_free(ctx, data);
		free(req);
		return JS_EXCEPTION;
	}

	data->algorithm_val = JS_DupValue(ctx, argv[0]);
	data->key_val = JS_DupValue(ctx, argv[1]);
	data->data_val = JS_DupValue(ctx, argv[2]);

	return nx_queue_async(ctx, req, nx_crypto_sign_do, nx_crypto_sign_cb);
}

// --- HMAC Verify ---

typedef struct {
	int err;
	JSValue algorithm_val;
	JSValue key_val;
	nx_crypto_key_t *key;
	JSValue signature_val;
	void *signature;
	size_t signature_size;
	JSValue data_val;
	void *data;
	size_t data_size;
	bool result;
} nx_crypto_verify_async_t;

void nx_crypto_verify_do(nx_work_t *req) {
	nx_crypto_verify_async_t *data = (nx_crypto_verify_async_t *)req->data;

	if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_HMAC) {
		nx_crypto_key_hmac_t *hmac = (nx_crypto_key_hmac_t *)data->key->handle;

		mbedtls_md_type_t md_type = nx_crypto_get_md_type(hmac->hash_name);
		if (md_type == MBEDTLS_MD_NONE) {
			data->err = ENOTSUP;
			return;
		}

		const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(md_type);
		size_t mac_size = mbedtls_md_get_size(md_info);
		uint8_t *computed = calloc(1, mac_size);
		if (!computed) {
			data->err = ENOMEM;
			return;
		}

		int ret = mbedtls_md_hmac(md_info, hmac->key, hmac->key_length,
								  data->data, data->data_size, computed);
		if (ret != 0) {
			free(computed);
			data->err = ret;
			return;
		}

		data->result = (data->signature_size == mac_size &&
						memcmp(computed, data->signature, mac_size) == 0);
		free(computed);
	} else {
		data->err = ENOTSUP;
	}
}

JSValue nx_crypto_verify_cb(JSContext *ctx, nx_work_t *req) {
	nx_crypto_verify_async_t *data = (nx_crypto_verify_async_t *)req->data;
	JS_FreeValue(ctx, data->algorithm_val);
	JS_FreeValue(ctx, data->key_val);
	JS_FreeValue(ctx, data->signature_val);
	JS_FreeValue(ctx, data->data_val);

	if (data->err) {
		JSValue err = JS_NewError(ctx);
		JS_DefinePropertyValueStr(ctx, err, "message",
								  JS_NewString(ctx, strerror(data->err)),
								  JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
		return JS_Throw(ctx, err);
	}

	return JS_NewBool(ctx, data->result);
}

static JSValue nx_crypto_verify(JSContext *ctx, JSValueConst this_val, int argc,
								JSValueConst *argv) {
	NX_INIT_WORK_T(nx_crypto_verify_async_t);

	data->key = JS_GetOpaque2(ctx, argv[1], nx_crypto_key_class_id);
	if (!data->key) {
		js_free(ctx, data);
		free(req);
		return JS_EXCEPTION;
	}

	if (!(data->key->usages & NX_CRYPTO_KEY_USAGE_VERIFY)) {
		js_free(ctx, data);
		free(req);
		return JS_ThrowTypeError(
			ctx, "Key does not support the 'verify' operation");
	}

	data->signature = NX_GetBufferSource(ctx, &data->signature_size, argv[2]);
	if (!data->signature) {
		js_free(ctx, data);
		free(req);
		return JS_EXCEPTION;
	}

	data->data = NX_GetBufferSource(ctx, &data->data_size, argv[3]);
	if (!data->data) {
		js_free(ctx, data);
		free(req);
		return JS_EXCEPTION;
	}

	data->algorithm_val = JS_DupValue(ctx, argv[0]);
	data->key_val = JS_DupValue(ctx, argv[1]);
	data->signature_val = JS_DupValue(ctx, argv[2]);
	data->data_val = JS_DupValue(ctx, argv[3]);

	return nx_queue_async(ctx, req, nx_crypto_verify_do, nx_crypto_verify_cb);
}

// --- Export Key ---

static JSValue nx_crypto_export_key(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	const char *format = JS_ToCString(ctx, argv[0]);
	if (!format) {
		return JS_EXCEPTION;
	}

	nx_crypto_key_t *key = JS_GetOpaque2(ctx, argv[1], nx_crypto_key_class_id);
	if (!key) {
		JS_FreeCString(ctx, format);
		return JS_EXCEPTION;
	}

	if (strcmp(format, "raw") != 0) {
		JS_FreeCString(ctx, format);
		return JS_ThrowTypeError(ctx,
								 "Only 'raw' export format is supported");
	}
	JS_FreeCString(ctx, format);

	if (!key->extractable) {
		return JS_ThrowTypeError(ctx, "Key is not extractable");
	}

	if (!key->raw_key_data || key->raw_key_size == 0) {
		return JS_ThrowTypeError(ctx, "Key does not have raw material");
	}

	// Return a copy of the raw key data
	uint8_t *copy = js_malloc(ctx, key->raw_key_size);
	if (!copy) {
		return JS_EXCEPTION;
	}
	memcpy(copy, key->raw_key_data, key->raw_key_size);
	return JS_NewArrayBuffer(ctx, copy, key->raw_key_size, free_array_buffer,
							 NULL, false);
}

// --- Derive Bits ---

typedef struct {
	int err;
	JSValue algorithm_val;
	JSValue key_val;
	nx_crypto_key_t *key;

	// Common
	char hash_name[16];

	// PBKDF2
	uint8_t *salt;
	size_t salt_size;
	uint32_t iterations;

	// HKDF
	uint8_t *info;
	size_t info_size;

	size_t length; // output length in bytes

	void *result;
	size_t result_size;
} nx_crypto_derive_bits_async_t;

void nx_crypto_derive_bits_do(nx_work_t *req) {
	nx_crypto_derive_bits_async_t *data =
		(nx_crypto_derive_bits_async_t *)req->data;

	mbedtls_md_type_t md_type = nx_crypto_get_md_type(data->hash_name);
	if (md_type == MBEDTLS_MD_NONE) {
		data->err = ENOTSUP;
		return;
	}

	const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(md_type);
	if (!md_info) {
		data->err = ENOTSUP;
		return;
	}

	data->result = calloc(1, data->length);
	if (!data->result) {
		data->err = ENOMEM;
		return;
	}
	data->result_size = data->length;

	int ret;
	if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_PBKDF2) {
		mbedtls_md_context_t md_ctx;
		mbedtls_md_init(&md_ctx);
		ret = mbedtls_md_setup(&md_ctx, md_info, 1);
		if (ret != 0) {
			mbedtls_md_free(&md_ctx);
			free(data->result);
			data->result = NULL;
			data->err = ret;
			return;
		}
		ret = mbedtls_pkcs5_pbkdf2_hmac(
			&md_ctx, data->key->raw_key_data, data->key->raw_key_size,
			data->salt, data->salt_size, data->iterations, data->length,
			data->result);
		mbedtls_md_free(&md_ctx);
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_HKDF) {
		ret = mbedtls_hkdf(md_info, data->salt, data->salt_size,
						   data->key->raw_key_data, data->key->raw_key_size,
						   data->info, data->info_size, data->result,
						   data->length);
	} else {
		ret = ENOTSUP;
	}

	if (ret != 0) {
		free(data->result);
		data->result = NULL;
		data->err = ret;
	}
}

JSValue nx_crypto_derive_bits_cb(JSContext *ctx, nx_work_t *req) {
	nx_crypto_derive_bits_async_t *data =
		(nx_crypto_derive_bits_async_t *)req->data;
	JS_FreeValue(ctx, data->algorithm_val);
	JS_FreeValue(ctx, data->key_val);

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

static JSValue nx_crypto_derive_bits(JSContext *ctx, JSValueConst this_val,
									 int argc, JSValueConst *argv) {
	NX_INIT_WORK_T(nx_crypto_derive_bits_async_t);

	data->key = JS_GetOpaque2(ctx, argv[1], nx_crypto_key_class_id);
	if (!data->key) {
		js_free(ctx, data);
		return JS_EXCEPTION;
	}

	if (!(data->key->usages & NX_CRYPTO_KEY_USAGE_DERIVE_BITS)) {
		js_free(ctx, data);
		return JS_ThrowTypeError(
			ctx, "Key does not support the 'deriveBits' operation");
	}

	uint32_t length_bits;
	if (JS_ToUint32(ctx, &length_bits, argv[2])) {
		js_free(ctx, data);
		return JS_EXCEPTION;
	}
	data->length = length_bits / 8;

	// Get hash from algorithm
	JSValue hash_val = JS_GetPropertyStr(ctx, argv[0], "hash");
	const char *hash_name;
	JSValue hash_name_val = JS_UNDEFINED;
	if (JS_IsString(hash_val)) {
		hash_name = JS_ToCString(ctx, hash_val);
	} else {
		hash_name_val = JS_GetPropertyStr(ctx, hash_val, "name");
		hash_name = JS_ToCString(ctx, hash_name_val);
		JS_FreeValue(ctx, hash_name_val);
	}
	JS_FreeValue(ctx, hash_val);
	if (!hash_name) {
		js_free(ctx, data);
		return JS_EXCEPTION;
	}
	strncpy(data->hash_name, hash_name, sizeof(data->hash_name) - 1);
	JS_FreeCString(ctx, hash_name);

	// Get salt
	JSValue salt_val = JS_GetPropertyStr(ctx, argv[0], "salt");
	data->salt = NX_GetBufferSource(ctx, &data->salt_size, salt_val);
	JS_FreeValue(ctx, salt_val);

	if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_PBKDF2) {
		if (!data->salt) {
			js_free(ctx, data);
			return JS_EXCEPTION;
		}
		uint32_t iterations;
		JSValue iter_val = JS_GetPropertyStr(ctx, argv[0], "iterations");
		if (JS_ToUint32(ctx, &iterations, iter_val)) {
			JS_FreeValue(ctx, iter_val);
			js_free(ctx, data);
			return JS_EXCEPTION;
		}
		JS_FreeValue(ctx, iter_val);
		data->iterations = iterations;
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_HKDF) {
		// salt can be empty for HKDF
		if (!data->salt) {
			data->salt = (uint8_t *)"";
			data->salt_size = 0;
		}
		JSValue info_val = JS_GetPropertyStr(ctx, argv[0], "info");
		data->info = NX_GetBufferSource(ctx, &data->info_size, info_val);
		JS_FreeValue(ctx, info_val);
		if (!data->info) {
			data->info = (uint8_t *)"";
			data->info_size = 0;
		}
	}

	data->algorithm_val = JS_DupValue(ctx, argv[0]);
	data->key_val = JS_DupValue(ctx, argv[1]);

	return nx_queue_async(ctx, req, nx_crypto_derive_bits_do,
						  nx_crypto_derive_bits_cb);
}

static JSValue nx_crypto_init(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_FUNC(proto, "getRandomValues", nx_crypto_get_random_values, 1);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static JSValue nx_crypto_subtle_init(JSContext *ctx, JSValueConst this_val,
									 int argc, JSValueConst *argv) {
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_FUNC(proto, "decrypt", nx_crypto_subtle_decrypt, 3);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("cryptoInit", 1, nx_crypto_init),
	JS_CFUNC_DEF("cryptoKeyNew", 1, nx_crypto_key_new),
	JS_CFUNC_DEF("cryptoKeyInit", 1, nx_crypto_key_init),
	JS_CFUNC_DEF("cryptoSubtleInit", 1, nx_crypto_subtle_init),
	JS_CFUNC_DEF("cryptoDigest", 0, nx_crypto_digest),
	JS_CFUNC_DEF("cryptoEncrypt", 0, nx_crypto_encrypt),
	JS_CFUNC_DEF("cryptoSign", 0, nx_crypto_sign),
	JS_CFUNC_DEF("cryptoVerify", 0, nx_crypto_verify),
	JS_CFUNC_DEF("cryptoExportKey", 0, nx_crypto_export_key),
	JS_CFUNC_DEF("cryptoDeriveBits", 0, nx_crypto_derive_bits),
	JS_CFUNC_DEF("sha256Hex", 0, nx_crypto_sha256_hex),
};

void nx_init_crypto(JSContext *ctx, JSValueConst init_obj) {
	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_crypto_key_class_id);
	JSClassDef crypto_key_class = {
		"CryptoKey",
		.finalizer = finalizer_crypto_key,
	};
	JS_NewClass(rt, nx_crypto_key_class_id, &crypto_key_class);

	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
