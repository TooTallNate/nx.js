#include "crypto.h"
#include "async.h"
#include "util.h"
#include <errno.h>
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
		if (context->handle) {
			js_free_rt(rt, context->handle);
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
			// aes192XtsContextResetTweak(&aes->encrypt.xts_192,
			//						   xts_params->tweak);
			//  aes192XtsEncrypt(&aes->encrypt.xts_192, data->result,
			//  data->data,
			//				 data->data_size);
		} else if (aes->key_length == 64) {
			data->err = ENOTSUP;
			// aes256XtsContextResetTweak(&aes->encrypt.xts_256,
			//						   xts_params->tweak);
			//  aes256XtsEncrypt(&aes->encrypt.xts_256, data->result,
			//  data->data,
			//				 data->data_size);
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
	case NX_CRYPTO_KEY_TYPE_PRIVATE:
		type = "private";
	case NX_CRYPTO_KEY_TYPE_PUBLIC:
		type = "public";
	case NX_CRYPTO_KEY_TYPE_SECRET:
		type = "secret";
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
		case NX_CRYPTO_KEY_ALGORITHM_AES_XTS:
			name_val = "AES-XTS";
			break;
		default:
			// TODO: throw error?
			break;
		}
		JS_SetPropertyStr(ctx, obj, "name", JS_NewString(ctx, name_val));

		if (context->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_CBC ||
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

	if (strcmp(algo, "AES-CBC") == 0) {
		if (key_size != 16 && key_size != 24 && key_size != 32) {
			js_free(ctx, context);
			JS_FreeCString(ctx, algo);
			JS_ThrowTypeError(ctx, "Invalid key size for AES-CBC");
			return JS_EXCEPTION;
		}
		context->type = NX_CRYPTO_KEY_TYPE_SECRET;
		context->algorithm = NX_CRYPTO_KEY_ALGORITHM_AES_CBC;

		nx_crypto_key_aes_t *aes = js_mallocz(ctx, sizeof(nx_crypto_key_aes_t));
		if (!aes) {
			js_free(ctx, context);
			JS_FreeCString(ctx, algo);
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
	} else if (strcmp(algo, "AES-XTS") == 0) {
		if (key_size != 32 && key_size != 48 && key_size != 64) {
			js_free(ctx, context);
			JS_FreeCString(ctx, algo);
			JS_ThrowTypeError(ctx, "Invalid key size for AES-XTS");
			return JS_EXCEPTION;
		}
		context->type = NX_CRYPTO_KEY_TYPE_SECRET;
		context->algorithm = NX_CRYPTO_KEY_ALGORITHM_AES_XTS;

		nx_crypto_key_aes_t *aes = js_mallocz(ctx, sizeof(nx_crypto_key_aes_t));
		if (!aes) {
			js_free(ctx, context);
			JS_FreeCString(ctx, algo);
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
	} else {
		JS_ThrowTypeError(ctx, "Unrecognized algorithm name: \"%s\"", algo);
		js_free(ctx, context);
		JS_FreeCString(ctx, algo);
		return JS_EXCEPTION;
	}

	JS_FreeCString(ctx, algo);

	JSValue obj = JS_NewObjectClass(ctx, nx_crypto_key_class_id);
	if (JS_IsException(obj)) {
		if (context->handle) {
			js_free(ctx, context->handle);
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
