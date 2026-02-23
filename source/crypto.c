#include "crypto.h"
#include "async.h"
#include "util.h"
#include <errno.h>
#include <mbedtls/error.h>
#include <mbedtls/version.h>
#include <mbedtls/gcm.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/hkdf.h>
#include <mbedtls/sha512.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/ecp.h>
#include <mbedtls/bignum.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/rsa.h>
#include <mbedtls/pk.h>
#include <mbedtls/base64.h>
#include <mbedtls/asn1write.h>
#include <mbedtls/asn1.h>
#include <mbedtls/oid.h>
#include <string.h>
#include <switch.h>

/* Forward declaration */
static mbedtls_md_type_t nx_crypto_get_md_type(const char *hash_name);

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

typedef struct {
	u8 *iv;
	size_t iv_size;
	u8 *additional_data;
	size_t additional_data_size;
	size_t tag_length; // in bytes (default 16 = 128 bits)
} nx_crypto_aes_gcm_params_t;

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
		if (context->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDSA ||
			context->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDH) {
			nx_crypto_key_ec_t *ec = context->handle;
			if (ec) {
				mbedtls_ecp_keypair_free(&ec->keypair);
			}
		}
		if (context->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_OAEP ||
			context->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_PSS ||
			context->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSASSA_PKCS1_V1_5) {
			nx_crypto_key_rsa_t *rsa = context->handle;
			if (rsa) {
				mbedtls_rsa_free(&rsa->rsa);
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
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_GCM) {
		nx_crypto_aes_gcm_params_t *gcm_params =
			(nx_crypto_aes_gcm_params_t *)data->algorithm_params;

		size_t tag_len = gcm_params->tag_length;
		data->result_size = data->data_size + tag_len;
		data->result = malloc(data->result_size);
		if (!data->result) {
			data->err = ENOMEM;
			return;
		}

		mbedtls_gcm_context gcm;
		mbedtls_gcm_init(&gcm);
		int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES,
									 data->key->raw_key_data,
									 data->key->raw_key_size * 8);
		if (ret != 0) {
			mbedtls_gcm_free(&gcm);
			free(data->result);
			data->result = NULL;
			data->err = ENOTSUP;
			return;
		}

		ret = mbedtls_gcm_crypt_and_tag(
			&gcm, MBEDTLS_GCM_ENCRYPT, data->data_size,
			gcm_params->iv, gcm_params->iv_size,
			gcm_params->additional_data, gcm_params->additional_data_size,
			data->data, data->result,
			tag_len, (unsigned char *)data->result + data->data_size);
		mbedtls_gcm_free(&gcm);

		if (ret != 0) {
			free(data->result);
			data->result = NULL;
			data->err = ENOTSUP;
			return;
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
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_OAEP) {
		nx_crypto_key_rsa_t *rsa = (nx_crypto_key_rsa_t *)data->key->handle;
		size_t rsa_len = mbedtls_rsa_get_len(&rsa->rsa);

		data->result_size = rsa_len;
		data->result = malloc(data->result_size);
		if (!data->result) {
			data->err = ENOMEM;
			return;
		}

		mbedtls_md_type_t md_type = nx_crypto_get_md_type(rsa->hash_name);

		mbedtls_entropy_context oaep_entropy;
		mbedtls_ctr_drbg_context oaep_ctr_drbg;
		mbedtls_entropy_init(&oaep_entropy);
		mbedtls_ctr_drbg_init(&oaep_ctr_drbg);
		int ret = mbedtls_ctr_drbg_seed(&oaep_ctr_drbg, mbedtls_entropy_func, &oaep_entropy, NULL, 0);
		if (ret != 0) {
			mbedtls_ctr_drbg_free(&oaep_ctr_drbg);
			mbedtls_entropy_free(&oaep_entropy);
			free(data->result);
			data->result = NULL;
			data->err = ret;
			return;
		}

		mbedtls_rsa_set_padding(&rsa->rsa, MBEDTLS_RSA_PKCS_V21, md_type);

		// label from algorithm_params (may be NULL)
		uint8_t *label = NULL;
		size_t label_len = 0;
		if (data->algorithm_params) {
			nx_crypto_aes_gcm_params_t *lp = (nx_crypto_aes_gcm_params_t *)data->algorithm_params;
			label = lp->iv;
			label_len = lp->iv_size;
		}

#if MBEDTLS_VERSION_MAJOR >= 3
		ret = mbedtls_rsa_rsaes_oaep_encrypt(&rsa->rsa, mbedtls_ctr_drbg_random,
			&oaep_ctr_drbg, (const unsigned char *)label, label_len, data->data_size, data->data, data->result);
#else
		ret = mbedtls_rsa_rsaes_oaep_encrypt(&rsa->rsa, mbedtls_ctr_drbg_random,
			&oaep_ctr_drbg, MBEDTLS_RSA_PUBLIC, (const unsigned char *)label, label_len, data->data_size, data->data, data->result);
#endif

		mbedtls_ctr_drbg_free(&oaep_ctr_drbg);
		mbedtls_entropy_free(&oaep_entropy);

		if (ret != 0) {
			free(data->result);
			data->result = NULL;
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
	if (!(data->key->usages & (NX_CRYPTO_KEY_USAGE_ENCRYPT | NX_CRYPTO_KEY_USAGE_WRAP_KEY))) {
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
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_GCM) {
		nx_crypto_aes_gcm_params_t *gcm_params =
			js_mallocz(ctx, sizeof(nx_crypto_aes_gcm_params_t));
		if (!gcm_params) {
			js_free(ctx, data);
			return JS_EXCEPTION;
		}

		gcm_params->iv = NX_GetBufferSource(
			ctx, &gcm_params->iv_size, JS_GetPropertyStr(ctx, argv[0], "iv"));
		if (!gcm_params->iv) {
			js_free(ctx, data);
			js_free(ctx, gcm_params);
			return JS_EXCEPTION;
		}

		// additionalData is optional
		JSValue ad_val = JS_GetPropertyStr(ctx, argv[0], "additionalData");
		if (!JS_IsUndefined(ad_val) && !JS_IsNull(ad_val)) {
			gcm_params->additional_data = NX_GetBufferSource(
				ctx, &gcm_params->additional_data_size, ad_val);
		} else {
			gcm_params->additional_data = NULL;
			gcm_params->additional_data_size = 0;
		}

		// tagLength is optional, defaults to 128 bits
		JSValue tag_val = JS_GetPropertyStr(ctx, argv[0], "tagLength");
		if (!JS_IsUndefined(tag_val) && !JS_IsNull(tag_val)) {
			u32 tag_bits;
			if (JS_ToUint32(ctx, &tag_bits, tag_val)) {
				js_free(ctx, data);
				js_free(ctx, gcm_params);
				return JS_EXCEPTION;
			}
			gcm_params->tag_length = tag_bits / 8;
		} else {
			gcm_params->tag_length = 16; // 128 bits default
		}

		data->algorithm_params = gcm_params;
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
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_OAEP) {
		// Optional label parameter
		JSValue label_val = JS_GetPropertyStr(ctx, argv[0], "label");
		if (!JS_IsUndefined(label_val) && !JS_IsNull(label_val)) {
			nx_crypto_aes_gcm_params_t *lp =
				js_mallocz(ctx, sizeof(nx_crypto_aes_gcm_params_t));
			if (!lp) {
				JS_FreeValue(ctx, label_val);
				js_free(ctx, data);
				return JS_EXCEPTION;
			}
			lp->iv = NX_GetBufferSource(ctx, &lp->iv_size, label_val);
			data->algorithm_params = lp;
		}
		JS_FreeValue(ctx, label_val);
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
	default:
		type = "unknown";
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
		case NX_CRYPTO_KEY_ALGORITHM_AES_GCM:
			name_val = "AES-GCM";
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
		case NX_CRYPTO_KEY_ALGORITHM_ECDSA: {
			name_val = "ECDSA";
			nx_crypto_key_ec_t *ec = (nx_crypto_key_ec_t *)context->handle;
			JS_SetPropertyStr(ctx, obj, "namedCurve",
							  JS_NewString(ctx, ec->curve_name));
			break;
		}
		case NX_CRYPTO_KEY_ALGORITHM_ECDH: {
			name_val = "ECDH";
			nx_crypto_key_ec_t *ec = (nx_crypto_key_ec_t *)context->handle;
			JS_SetPropertyStr(ctx, obj, "namedCurve",
							  JS_NewString(ctx, ec->curve_name));
			break;
		}
		case NX_CRYPTO_KEY_ALGORITHM_RSA_OAEP: {
			name_val = "RSA-OAEP";
			nx_crypto_key_rsa_t *rsa = (nx_crypto_key_rsa_t *)context->handle;
			JS_SetPropertyStr(ctx, obj, "modulusLength",
							  JS_NewUint32(ctx, mbedtls_rsa_get_len(&rsa->rsa) * 8));
			JSValue hash_obj = JS_NewObject(ctx);
			JS_SetPropertyStr(ctx, hash_obj, "name",
							  JS_NewString(ctx, rsa->hash_name));
			JS_SetPropertyStr(ctx, obj, "hash", hash_obj);
			// publicExponent as Uint8Array
			mbedtls_mpi E;
			mbedtls_mpi_init(&E);
			mbedtls_rsa_export(&rsa->rsa, NULL, NULL, NULL, NULL, &E);
			size_t pe_len = mbedtls_mpi_size(&E);
			uint8_t *pe_copy = malloc(pe_len);
			if (pe_copy) {
				mbedtls_mpi_write_binary(&E, pe_copy, pe_len);
				JS_SetPropertyStr(ctx, obj, "publicExponent",
					JS_NewArrayBuffer(ctx, pe_copy, pe_len, free_array_buffer, NULL, false));
			}
			mbedtls_mpi_free(&E);
			break;
		}
		case NX_CRYPTO_KEY_ALGORITHM_RSA_PSS: {
			name_val = "RSA-PSS";
			nx_crypto_key_rsa_t *rsa = (nx_crypto_key_rsa_t *)context->handle;
			JS_SetPropertyStr(ctx, obj, "modulusLength",
							  JS_NewUint32(ctx, mbedtls_rsa_get_len(&rsa->rsa) * 8));
			JSValue hash_obj = JS_NewObject(ctx);
			JS_SetPropertyStr(ctx, hash_obj, "name",
							  JS_NewString(ctx, rsa->hash_name));
			JS_SetPropertyStr(ctx, obj, "hash", hash_obj);
			mbedtls_mpi E2;
			mbedtls_mpi_init(&E2);
			mbedtls_rsa_export(&rsa->rsa, NULL, NULL, NULL, NULL, &E2);
			size_t pe_len2 = mbedtls_mpi_size(&E2);
			uint8_t *pe_copy2 = malloc(pe_len2);
			if (pe_copy2) {
				mbedtls_mpi_write_binary(&E2, pe_copy2, pe_len2);
				JS_SetPropertyStr(ctx, obj, "publicExponent",
					JS_NewArrayBuffer(ctx, pe_copy2, pe_len2, free_array_buffer, NULL, false));
			}
			mbedtls_mpi_free(&E2);
			break;
		}
		case NX_CRYPTO_KEY_ALGORITHM_RSASSA_PKCS1_V1_5: {
			name_val = "RSASSA-PKCS1-v1_5";
			nx_crypto_key_rsa_t *rsa = (nx_crypto_key_rsa_t *)context->handle;
			JS_SetPropertyStr(ctx, obj, "modulusLength",
							  JS_NewUint32(ctx, mbedtls_rsa_get_len(&rsa->rsa) * 8));
			JSValue hash_obj = JS_NewObject(ctx);
			JS_SetPropertyStr(ctx, hash_obj, "name",
							  JS_NewString(ctx, rsa->hash_name));
			JS_SetPropertyStr(ctx, obj, "hash", hash_obj);
			mbedtls_mpi E3;
			mbedtls_mpi_init(&E3);
			mbedtls_rsa_export(&rsa->rsa, NULL, NULL, NULL, NULL, &E3);
			size_t pe_len3 = mbedtls_mpi_size(&E3);
			uint8_t *pe_copy3 = malloc(pe_len3);
			if (pe_copy3) {
				mbedtls_mpi_write_binary(&E3, pe_copy3, pe_len3);
				JS_SetPropertyStr(ctx, obj, "publicExponent",
					JS_NewArrayBuffer(ctx, pe_copy3, pe_len3, free_array_buffer, NULL, false));
			}
			mbedtls_mpi_free(&E3);
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
		} else if (context->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_GCM) {
			JS_SetPropertyStr(ctx, obj, "length",
							  JS_NewUint32(ctx, context->raw_key_size * 8));
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
	} else if (strcmp(algo, "AES-GCM") == 0) {
		if (key_size != 16 && key_size != 24 && key_size != 32) {
			js_free(ctx, context->raw_key_data);
			js_free(ctx, context);
			JS_FreeCString(ctx, algo);
			JS_FreeValue(ctx, algo_val);
			JS_ThrowPlainError(ctx, "Invalid key length");
			return JS_EXCEPTION;
		}
		context->type = NX_CRYPTO_KEY_TYPE_SECRET;
		context->algorithm = NX_CRYPTO_KEY_ALGORITHM_AES_GCM;
		// AES-GCM uses mbedtls directly, no libnx handle needed
		// raw_key_data is already stored above
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
	} else if (strcmp(algo, "ECDSA") == 0 || strcmp(algo, "ECDH") == 0) {
		// Import EC public key from raw format (uncompressed point: 0x04 || x || y)
		context->type = NX_CRYPTO_KEY_TYPE_PUBLIC;
		context->algorithm = (strcmp(algo, "ECDSA") == 0)
			? NX_CRYPTO_KEY_ALGORITHM_ECDSA
			: NX_CRYPTO_KEY_ALGORITHM_ECDH;

		JSValue curve_val = JS_GetPropertyStr(ctx, argv[0], "namedCurve");
		const char *curve_name = JS_ToCString(ctx, curve_val);
		JS_FreeValue(ctx, curve_val);
		if (!curve_name) {
			js_free(ctx, context->raw_key_data);
			js_free(ctx, context);
			JS_FreeValue(ctx, algo_val);
			JS_FreeCString(ctx, algo);
			return JS_EXCEPTION;
		}

		mbedtls_ecp_group_id grp_id;
		if (strcmp(curve_name, "P-256") == 0) {
			grp_id = MBEDTLS_ECP_DP_SECP256R1;
		} else if (strcmp(curve_name, "P-384") == 0) {
			grp_id = MBEDTLS_ECP_DP_SECP384R1;
		} else {
			JS_ThrowTypeError(ctx, "Unsupported curve: %s", curve_name);
			JS_FreeCString(ctx, curve_name);
			js_free(ctx, context->raw_key_data);
			js_free(ctx, context);
			JS_FreeValue(ctx, algo_val);
			JS_FreeCString(ctx, algo);
			return JS_EXCEPTION;
		}

		nx_crypto_key_ec_t *ec = js_mallocz(ctx, sizeof(nx_crypto_key_ec_t));
		if (!ec) {
			JS_FreeCString(ctx, curve_name);
			js_free(ctx, context->raw_key_data);
			js_free(ctx, context);
			JS_FreeValue(ctx, algo_val);
			JS_FreeCString(ctx, algo);
			return JS_EXCEPTION;
		}
		mbedtls_ecp_keypair_init(&ec->keypair);
		strncpy(ec->curve_name, curve_name, sizeof(ec->curve_name) - 1);
		JS_FreeCString(ctx, curve_name);

		int ret = mbedtls_ecp_group_load(&ec->keypair.grp, grp_id);
		if (ret != 0) {
			mbedtls_ecp_keypair_free(&ec->keypair);
			js_free(ctx, ec);
			js_free(ctx, context->raw_key_data);
			js_free(ctx, context);
			JS_FreeValue(ctx, algo_val);
			JS_FreeCString(ctx, algo);
			JS_ThrowPlainError(ctx, "Failed to load EC group");
			return JS_EXCEPTION;
		}

		ret = mbedtls_ecp_point_read_binary(&ec->keypair.grp, &ec->keypair.Q,
											 key_data, key_size);
		if (ret != 0) {
			mbedtls_ecp_keypair_free(&ec->keypair);
			js_free(ctx, ec);
			js_free(ctx, context->raw_key_data);
			js_free(ctx, context);
			JS_FreeValue(ctx, algo_val);
			JS_FreeCString(ctx, algo);
			JS_ThrowPlainError(ctx, "Failed to read EC public key");
			return JS_EXCEPTION;
		}

		context->handle = ec;
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
		if (context->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDSA ||
			context->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDH) {
			nx_crypto_key_ec_t *ec = context->handle;
			if (ec) {
				mbedtls_ecp_keypair_free(&ec->keypair);
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
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_GCM) {
		nx_crypto_aes_gcm_params_t *gcm_params =
			(nx_crypto_aes_gcm_params_t *)data->algorithm_params;

		size_t tag_len = gcm_params->tag_length;
		if (data->data_size < tag_len) {
			data->err = EINVAL;
			return;
		}

		size_t ciphertext_size = data->data_size - tag_len;
		data->result_size = ciphertext_size;
		data->result = malloc(data->result_size);
		if (!data->result) {
			data->err = ENOMEM;
			return;
		}

		mbedtls_gcm_context gcm;
		mbedtls_gcm_init(&gcm);
		int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES,
									 data->key->raw_key_data,
									 data->key->raw_key_size * 8);
		if (ret != 0) {
			mbedtls_gcm_free(&gcm);
			free(data->result);
			data->result = NULL;
			data->err = ENOTSUP;
			return;
		}

		ret = mbedtls_gcm_auth_decrypt(
			&gcm, ciphertext_size,
			gcm_params->iv, gcm_params->iv_size,
			gcm_params->additional_data, gcm_params->additional_data_size,
			(const unsigned char *)data->data + ciphertext_size, tag_len,
			data->data, data->result);
		mbedtls_gcm_free(&gcm);

		if (ret != 0) {
			free(data->result);
			data->result = NULL;
			// Auth tag verification failed — OperationError
			data->err = EACCES;
			return;
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
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_OAEP) {
		nx_crypto_key_rsa_t *rsa = (nx_crypto_key_rsa_t *)data->key->handle;
		size_t rsa_len = mbedtls_rsa_get_len(&rsa->rsa);

		mbedtls_md_type_t md_type = nx_crypto_get_md_type(rsa->hash_name);

		mbedtls_entropy_context dec_entropy;
		mbedtls_ctr_drbg_context dec_ctr_drbg;
		mbedtls_entropy_init(&dec_entropy);
		mbedtls_ctr_drbg_init(&dec_ctr_drbg);
		int ret = mbedtls_ctr_drbg_seed(&dec_ctr_drbg, mbedtls_entropy_func, &dec_entropy, NULL, 0);
		if (ret != 0) {
			mbedtls_ctr_drbg_free(&dec_ctr_drbg);
			mbedtls_entropy_free(&dec_entropy);
			data->err = ret;
			return;
		}

		mbedtls_rsa_set_padding(&rsa->rsa, MBEDTLS_RSA_PKCS_V21, md_type);

		uint8_t *label = NULL;
		size_t label_len = 0;
		if (data->algorithm_params) {
			nx_crypto_aes_gcm_params_t *lp = (nx_crypto_aes_gcm_params_t *)data->algorithm_params;
			label = lp->iv;
			label_len = lp->iv_size;
		}

		uint8_t *output = malloc(rsa_len);
		if (!output) {
			mbedtls_ctr_drbg_free(&dec_ctr_drbg);
			mbedtls_entropy_free(&dec_entropy);
			data->err = ENOMEM;
			return;
		}

		size_t olen = 0;
#if MBEDTLS_VERSION_MAJOR >= 3
		ret = mbedtls_rsa_rsaes_oaep_decrypt(&rsa->rsa, mbedtls_ctr_drbg_random,
			&dec_ctr_drbg, (const unsigned char *)label, label_len, &olen, data->data, output, rsa_len);
#else
		ret = mbedtls_rsa_rsaes_oaep_decrypt(&rsa->rsa, mbedtls_ctr_drbg_random,
			&dec_ctr_drbg, MBEDTLS_RSA_PRIVATE, (const unsigned char *)label, label_len, &olen, data->data, output, rsa_len);
#endif

		mbedtls_ctr_drbg_free(&dec_ctr_drbg);
		mbedtls_entropy_free(&dec_entropy);

		if (ret != 0) {
			free(output);
			data->err = EACCES;
			return;
		}

		data->result = malloc(olen);
		if (!data->result) {
			free(output);
			data->err = ENOMEM;
			return;
		}
		memcpy(data->result, output, olen);
		data->result_size = olen;
		free(output);
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
	if (!(data->key->usages & (NX_CRYPTO_KEY_USAGE_DECRYPT | NX_CRYPTO_KEY_USAGE_UNWRAP_KEY))) {
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
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_GCM) {
		nx_crypto_aes_gcm_params_t *gcm_params =
			js_mallocz(ctx, sizeof(nx_crypto_aes_gcm_params_t));
		if (!gcm_params) {
			js_free(ctx, data);
			return JS_EXCEPTION;
		}

		gcm_params->iv = NX_GetBufferSource(
			ctx, &gcm_params->iv_size, JS_GetPropertyStr(ctx, argv[0], "iv"));
		if (!gcm_params->iv) {
			js_free(ctx, data);
			js_free(ctx, gcm_params);
			return JS_EXCEPTION;
		}

		JSValue ad_val = JS_GetPropertyStr(ctx, argv[0], "additionalData");
		if (!JS_IsUndefined(ad_val) && !JS_IsNull(ad_val)) {
			gcm_params->additional_data = NX_GetBufferSource(
				ctx, &gcm_params->additional_data_size, ad_val);
		} else {
			gcm_params->additional_data = NULL;
			gcm_params->additional_data_size = 0;
		}

		JSValue tag_val = JS_GetPropertyStr(ctx, argv[0], "tagLength");
		if (!JS_IsUndefined(tag_val) && !JS_IsNull(tag_val)) {
			u32 tag_bits;
			if (JS_ToUint32(ctx, &tag_bits, tag_val)) {
				js_free(ctx, data);
				js_free(ctx, gcm_params);
				return JS_EXCEPTION;
			}
			gcm_params->tag_length = tag_bits / 8;
		} else {
			gcm_params->tag_length = 16;
		}

		data->algorithm_params = gcm_params;
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
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_OAEP) {
		JSValue label_val = JS_GetPropertyStr(ctx, argv[0], "label");
		if (!JS_IsUndefined(label_val) && !JS_IsNull(label_val)) {
			nx_crypto_aes_gcm_params_t *lp =
				js_mallocz(ctx, sizeof(nx_crypto_aes_gcm_params_t));
			if (!lp) {
				JS_FreeValue(ctx, label_val);
				js_free(ctx, data);
				return JS_EXCEPTION;
			}
			lp->iv = NX_GetBufferSource(ctx, &lp->iv_size, label_val);
			data->algorithm_params = lp;
		}
		JS_FreeValue(ctx, label_val);
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
	void *algorithm_params;
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
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDSA) {
		nx_crypto_key_ec_t *ec = (nx_crypto_key_ec_t *)data->key->handle;

		// Get hash algorithm from algorithm_params (stored as string)
		const char *hash_name = (const char *)data->algorithm_params;
		mbedtls_md_type_t md_type = nx_crypto_get_md_type(hash_name);
		if (md_type == MBEDTLS_MD_NONE) {
			data->err = ENOTSUP;
			return;
		}

		// Hash the data
		const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(md_type);
		size_t hash_len = mbedtls_md_get_size(md_info);
		uint8_t hash[64]; // max SHA-512
		int ret = mbedtls_md(md_info, data->data, data->data_size, hash);
		if (ret != 0) {
			data->err = ret;
			return;
		}

		// Sign with ECDSA (DER format from mbedtls)
		uint8_t der_sig[256];
		size_t der_sig_len = 0;

		mbedtls_entropy_context entropy;
		mbedtls_ctr_drbg_context ctr_drbg;
		mbedtls_entropy_init(&entropy);
		mbedtls_ctr_drbg_init(&ctr_drbg);
		ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
									 NULL, 0);
		if (ret != 0) {
			mbedtls_ctr_drbg_free(&ctr_drbg);
			mbedtls_entropy_free(&entropy);
			data->err = ret;
			return;
		}

#if MBEDTLS_VERSION_MAJOR >= 3
		ret = mbedtls_ecdsa_write_signature(&ec->keypair, md_type,
											hash, hash_len,
											der_sig, sizeof(der_sig), &der_sig_len,
											mbedtls_ctr_drbg_random, &ctr_drbg);
#else
		ret = mbedtls_ecdsa_write_signature(&ec->keypair, md_type,
											hash, hash_len,
											der_sig, &der_sig_len,
											mbedtls_ctr_drbg_random, &ctr_drbg);
#endif
		mbedtls_ctr_drbg_free(&ctr_drbg);
		mbedtls_entropy_free(&entropy);

		if (ret != 0) {
			data->err = ret;
			return;
		}

		// Convert DER to IEEE P1363 (r||s) format
		size_t coord_size = mbedtls_mpi_size(&ec->keypair.grp.P);
		data->result_size = coord_size * 2;
		data->result = calloc(1, data->result_size);
		if (!data->result) {
			data->err = ENOMEM;
			return;
		}

		// Parse DER: SEQUENCE { INTEGER r, INTEGER s }
		mbedtls_mpi r, s;
		mbedtls_mpi_init(&r);
		mbedtls_mpi_init(&s);

		uint8_t *p = der_sig;
		size_t len;

		// Skip SEQUENCE tag and length
		if (*p != 0x30) { data->err = EINVAL; goto sign_cleanup; }
		p++;
		if (*p & 0x80) { p += (*p & 0x7f) + 1; } else { p++; }

		// Read r INTEGER
		if (*p != 0x02) { data->err = EINVAL; goto sign_cleanup; }
		p++;
		len = *p++; // length of r
		ret = mbedtls_mpi_read_binary(&r, p, len);
		if (ret != 0) { data->err = ret; goto sign_cleanup; }
		p += len;

		// Read s INTEGER
		if (*p != 0x02) { data->err = EINVAL; goto sign_cleanup; }
		p++;
		len = *p++; // length of s
		ret = mbedtls_mpi_read_binary(&s, p, len);
		if (ret != 0) { data->err = ret; goto sign_cleanup; }

		// Write r and s as fixed-size big-endian
		ret = mbedtls_mpi_write_binary(&r, data->result, coord_size);
		if (ret != 0) { data->err = ret; goto sign_cleanup; }
		ret = mbedtls_mpi_write_binary(&s, (uint8_t *)data->result + coord_size, coord_size);
		if (ret != 0) { data->err = ret; goto sign_cleanup; }

sign_cleanup:
		mbedtls_mpi_free(&r);
		mbedtls_mpi_free(&s);
		if (data->err) {
			free(data->result);
			data->result = NULL;
		}
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSASSA_PKCS1_V1_5) {
		nx_crypto_key_rsa_t *rsa = (nx_crypto_key_rsa_t *)data->key->handle;
		mbedtls_md_type_t md_type = nx_crypto_get_md_type(rsa->hash_name);
		if (md_type == MBEDTLS_MD_NONE) { data->err = ENOTSUP; return; }

		const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(md_type);
		size_t hash_len = mbedtls_md_get_size(md_info);
		uint8_t hash_buf[64];
		int ret = mbedtls_md(md_info, data->data, data->data_size, hash_buf);
		if (ret != 0) { data->err = ret; return; }

		size_t rsa_len = mbedtls_rsa_get_len(&rsa->rsa);
		data->result = malloc(rsa_len);
		if (!data->result) { data->err = ENOMEM; return; }
		data->result_size = rsa_len;

		mbedtls_rsa_set_padding(&rsa->rsa, MBEDTLS_RSA_PKCS_V15, MBEDTLS_MD_NONE);
		mbedtls_entropy_context sign_entropy;
		mbedtls_ctr_drbg_context sign_ctr_drbg;
		mbedtls_entropy_init(&sign_entropy);
		mbedtls_ctr_drbg_init(&sign_ctr_drbg);
		ret = mbedtls_ctr_drbg_seed(&sign_ctr_drbg, mbedtls_entropy_func,
							  &sign_entropy, NULL, 0);
		if (ret != 0) {
			mbedtls_ctr_drbg_free(&sign_ctr_drbg);
			mbedtls_entropy_free(&sign_entropy);
			free(data->result);
			data->result = NULL;
			data->err = ret;
			return;
		}

#if MBEDTLS_VERSION_MAJOR >= 3
		ret = mbedtls_rsa_rsassa_pkcs1_v15_sign(&rsa->rsa,
			mbedtls_ctr_drbg_random, &sign_ctr_drbg,
			md_type, hash_len, hash_buf, data->result);
#else
		ret = mbedtls_rsa_rsassa_pkcs1_v15_sign(&rsa->rsa,
			mbedtls_ctr_drbg_random, &sign_ctr_drbg,
			MBEDTLS_RSA_PRIVATE, md_type, hash_len, hash_buf, data->result);
#endif
		mbedtls_ctr_drbg_free(&sign_ctr_drbg);
		mbedtls_entropy_free(&sign_entropy);

		if (ret != 0) {
			free(data->result);
			data->result = NULL;
			data->err = ret;
		}
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_PSS) {
		nx_crypto_key_rsa_t *rsa = (nx_crypto_key_rsa_t *)data->key->handle;
		mbedtls_md_type_t md_type = nx_crypto_get_md_type(rsa->hash_name);
		if (md_type == MBEDTLS_MD_NONE) { data->err = ENOTSUP; return; }

		const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(md_type);
		size_t hash_len = mbedtls_md_get_size(md_info);
		uint8_t hash_buf2[64];
		int ret = mbedtls_md(md_info, data->data, data->data_size, hash_buf2);
		if (ret != 0) { data->err = ret; return; }

		size_t rsa_len = mbedtls_rsa_get_len(&rsa->rsa);
		data->result = malloc(rsa_len);
		if (!data->result) { data->err = ENOMEM; return; }
		data->result_size = rsa_len;

		// Get salt length from algorithm_params or default to hash length
		int salt_len = rsa->salt_length;
		if (data->algorithm_params) {
			salt_len = *(int *)data->algorithm_params;
		}
		if (salt_len < 0) salt_len = (int)hash_len;

		mbedtls_entropy_context pss_entropy;
		mbedtls_ctr_drbg_context pss_ctr_drbg;
		mbedtls_entropy_init(&pss_entropy);
		mbedtls_ctr_drbg_init(&pss_ctr_drbg);
		ret = mbedtls_ctr_drbg_seed(&pss_ctr_drbg, mbedtls_entropy_func, &pss_entropy, NULL, 0);
		if (ret != 0) {
			mbedtls_ctr_drbg_free(&pss_ctr_drbg);
			mbedtls_entropy_free(&pss_entropy);
			free(data->result);
			data->result = NULL;
			data->err = ret;
			return;
		}

		mbedtls_rsa_set_padding(&rsa->rsa, MBEDTLS_RSA_PKCS_V21, md_type);
#if MBEDTLS_VERSION_MAJOR >= 3
		ret = mbedtls_rsa_rsassa_pss_sign_ext(&rsa->rsa, mbedtls_ctr_drbg_random,
			&pss_ctr_drbg, md_type, hash_len, hash_buf2, salt_len, data->result);
#else
		if (salt_len != (int)hash_len) {
			mbedtls_ctr_drbg_free(&pss_ctr_drbg);
			mbedtls_entropy_free(&pss_entropy);
			free(data->result);
			data->result = NULL;
			data->err = ENOTSUP;
			return;
		}
		ret = mbedtls_rsa_rsassa_pss_sign(&rsa->rsa, mbedtls_ctr_drbg_random,
			&pss_ctr_drbg, MBEDTLS_RSA_PRIVATE, md_type, hash_len, hash_buf2, data->result);
#endif

		mbedtls_ctr_drbg_free(&pss_ctr_drbg);
		mbedtls_entropy_free(&pss_entropy);

		if (ret != 0) {
			free(data->result);
			data->result = NULL;
			data->err = ENOTSUP;
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
	if (data->algorithm_params) free(data->algorithm_params);

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

	// For ECDSA, extract hash algorithm name
	if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDSA) {
		JSValue hash_val = JS_GetPropertyStr(ctx, argv[0], "hash");
		const char *hash_str;
		JSValue hash_name_val = JS_UNDEFINED;
		if (JS_IsString(hash_val)) {
			hash_str = JS_ToCString(ctx, hash_val);
		} else {
			hash_name_val = JS_GetPropertyStr(ctx, hash_val, "name");
			hash_str = JS_ToCString(ctx, hash_name_val);
			JS_FreeValue(ctx, hash_name_val);
		}
		JS_FreeValue(ctx, hash_val);
		if (!hash_str) {
			js_free(ctx, data);
			free(req);
			return JS_EXCEPTION;
		}
		char *hash_copy = malloc(strlen(hash_str) + 1);
		strcpy(hash_copy, hash_str);
		JS_FreeCString(ctx, hash_str);
		data->algorithm_params = hash_copy;
	}

	// For RSA-PSS, extract saltLength
	if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_PSS) {
		JSValue salt_val = JS_GetPropertyStr(ctx, argv[0], "saltLength");
		if (!JS_IsUndefined(salt_val)) {
			int *salt_len_ptr = malloc(sizeof(int));
			u32 sl;
			if (JS_ToUint32(ctx, &sl, salt_val) == 0) {
				*salt_len_ptr = (int)sl;
			} else {
				*salt_len_ptr = -1;
			}
			data->algorithm_params = salt_len_ptr;
		}
		JS_FreeValue(ctx, salt_val);
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
	void *algorithm_params;
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
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDSA) {
		nx_crypto_key_ec_t *ec = (nx_crypto_key_ec_t *)data->key->handle;

		const char *hash_name = (const char *)data->algorithm_params;
		mbedtls_md_type_t md_type = nx_crypto_get_md_type(hash_name);
		if (md_type == MBEDTLS_MD_NONE) {
			data->err = ENOTSUP;
			return;
		}

		// Hash the data
		const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(md_type);
		size_t hash_len = mbedtls_md_get_size(md_info);
		uint8_t hash[64];
		int ret = mbedtls_md(md_info, data->data, data->data_size, hash);
		if (ret != 0) {
			data->err = ret;
			return;
		}

		// Convert P1363 (r||s) to DER format for mbedtls
		size_t coord_size = mbedtls_mpi_size(&ec->keypair.grp.P);
		if (data->signature_size != coord_size * 2) {
			data->result = false;
			return;
		}

		mbedtls_mpi r, s;
		mbedtls_mpi_init(&r);
		mbedtls_mpi_init(&s);
		mbedtls_mpi_read_binary(&r, data->signature, coord_size);
		mbedtls_mpi_read_binary(&s, (uint8_t *)data->signature + coord_size, coord_size);

		ret = mbedtls_ecdsa_verify(&ec->keypair.grp, hash, hash_len,
								   &ec->keypair.Q, &r, &s);
		data->result = (ret == 0);

		mbedtls_mpi_free(&r);
		mbedtls_mpi_free(&s);
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSASSA_PKCS1_V1_5) {
		nx_crypto_key_rsa_t *rsa = (nx_crypto_key_rsa_t *)data->key->handle;
		mbedtls_md_type_t md_type = nx_crypto_get_md_type(rsa->hash_name);
		if (md_type == MBEDTLS_MD_NONE) { data->err = ENOTSUP; return; }

		const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(md_type);
		size_t hash_len = mbedtls_md_get_size(md_info);
		uint8_t hash_buf[64];
		int ret = mbedtls_md(md_info, data->data, data->data_size, hash_buf);
		if (ret != 0) { data->err = ret; return; }

		mbedtls_rsa_set_padding(&rsa->rsa, MBEDTLS_RSA_PKCS_V15, MBEDTLS_MD_NONE);
#if MBEDTLS_VERSION_MAJOR >= 3
		ret = mbedtls_rsa_rsassa_pkcs1_v15_verify(&rsa->rsa,
			md_type, hash_len, hash_buf, data->signature);
#else
		ret = mbedtls_rsa_rsassa_pkcs1_v15_verify(&rsa->rsa, NULL, NULL,
			MBEDTLS_RSA_PUBLIC, md_type, hash_len, hash_buf, data->signature);
#endif
		data->result = (ret == 0);
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_PSS) {
		nx_crypto_key_rsa_t *rsa = (nx_crypto_key_rsa_t *)data->key->handle;
		mbedtls_md_type_t md_type = nx_crypto_get_md_type(rsa->hash_name);
		if (md_type == MBEDTLS_MD_NONE) { data->err = ENOTSUP; return; }

		const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(md_type);
		size_t hash_len = mbedtls_md_get_size(md_info);
		uint8_t hash_buf2[64];
		int ret = mbedtls_md(md_info, data->data, data->data_size, hash_buf2);
		if (ret != 0) { data->err = ret; return; }

		int salt_len = rsa->salt_length;
		if (data->algorithm_params) {
			salt_len = *(int *)data->algorithm_params;
		}
		if (salt_len < 0) salt_len = (int)hash_len;

		mbedtls_rsa_set_padding(&rsa->rsa, MBEDTLS_RSA_PKCS_V21, md_type);
#if MBEDTLS_VERSION_MAJOR >= 3
		ret = mbedtls_rsa_rsassa_pss_verify_ext(&rsa->rsa,
			md_type, hash_len, hash_buf2, md_type, salt_len, data->signature);
#else
		ret = mbedtls_rsa_rsassa_pss_verify_ext(&rsa->rsa, NULL, NULL,
			MBEDTLS_RSA_PUBLIC, md_type, hash_len, hash_buf2, md_type, salt_len, data->signature);
#endif
		data->result = (ret == 0);
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
	if (data->algorithm_params) free(data->algorithm_params);

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

	// For ECDSA, extract hash algorithm name
	if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDSA) {
		JSValue hash_val = JS_GetPropertyStr(ctx, argv[0], "hash");
		const char *hash_str;
		JSValue hash_name_val = JS_UNDEFINED;
		if (JS_IsString(hash_val)) {
			hash_str = JS_ToCString(ctx, hash_val);
		} else {
			hash_name_val = JS_GetPropertyStr(ctx, hash_val, "name");
			hash_str = JS_ToCString(ctx, hash_name_val);
			JS_FreeValue(ctx, hash_name_val);
		}
		JS_FreeValue(ctx, hash_val);
		if (!hash_str) {
			js_free(ctx, data);
			free(req);
			return JS_EXCEPTION;
		}
		char *hash_copy = malloc(strlen(hash_str) + 1);
		strcpy(hash_copy, hash_str);
		JS_FreeCString(ctx, hash_str);
		data->algorithm_params = hash_copy;
	}

	// For RSA-PSS, extract saltLength
	if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_PSS) {
		JSValue salt_val = JS_GetPropertyStr(ctx, argv[0], "saltLength");
		if (!JS_IsUndefined(salt_val)) {
			int *salt_len_ptr = malloc(sizeof(int));
			u32 sl;
			if (JS_ToUint32(ctx, &sl, salt_val) == 0) {
				*salt_len_ptr = (int)sl;
			} else {
				*salt_len_ptr = -1;
			}
			data->algorithm_params = salt_len_ptr;
		}
		JS_FreeValue(ctx, salt_val);
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

// --- EC Generate Key ---
// Returns an array: [publicKeyRaw, privateKeyRaw]
// where publicKeyRaw is uncompressed point and privateKeyRaw is the scalar d
static JSValue nx_crypto_generate_key_ec(JSContext *ctx, JSValueConst this_val,
										  int argc, JSValueConst *argv) {
	const char *curve_name = JS_ToCString(ctx, argv[0]);
	if (!curve_name) return JS_EXCEPTION;

	mbedtls_ecp_group_id grp_id;
	if (strcmp(curve_name, "P-256") == 0) {
		grp_id = MBEDTLS_ECP_DP_SECP256R1;
	} else if (strcmp(curve_name, "P-384") == 0) {
		grp_id = MBEDTLS_ECP_DP_SECP384R1;
	} else {
		JS_FreeCString(ctx, curve_name);
		return JS_ThrowTypeError(ctx, "Unsupported curve");
	}
	JS_FreeCString(ctx, curve_name);

	mbedtls_ecp_keypair kp;
	mbedtls_ecp_keypair_init(&kp);

	int ret = mbedtls_ecp_group_load(&kp.grp, grp_id);
	if (ret != 0) {
		mbedtls_ecp_keypair_free(&kp);
		return JS_ThrowPlainError(ctx, "Failed to load EC group: %d", ret);
	}

	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_entropy_init(&entropy);
	mbedtls_ctr_drbg_init(&ctr_drbg);
	ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
	if (ret != 0) {
		mbedtls_ctr_drbg_free(&ctr_drbg);
		mbedtls_entropy_free(&entropy);
		mbedtls_ecp_keypair_free(&kp);
		return JS_ThrowPlainError(ctx, "Failed to seed DRBG: %d", ret);
	}

	ret = mbedtls_ecp_gen_keypair(&kp.grp, &kp.d, &kp.Q,
								   mbedtls_ctr_drbg_random, &ctr_drbg);
	mbedtls_ctr_drbg_free(&ctr_drbg);
	mbedtls_entropy_free(&entropy);

	if (ret != 0) {
		mbedtls_ecp_keypair_free(&kp);
		return JS_ThrowPlainError(ctx, "Failed to generate EC keypair: %d", ret);
	}

	// Export public key as uncompressed point
	size_t pub_len = 0;
	size_t coord_size = mbedtls_mpi_size(&kp.grp.P);
	size_t pub_buf_size = 1 + 2 * coord_size;
	uint8_t *pub_buf = malloc(pub_buf_size);
	if (!pub_buf) {
		mbedtls_ecp_keypair_free(&kp);
		return JS_EXCEPTION;
	}
	ret = mbedtls_ecp_point_write_binary(&kp.grp, &kp.Q,
										  MBEDTLS_ECP_PF_UNCOMPRESSED,
										  &pub_len, pub_buf, pub_buf_size);
	if (ret != 0) {
		free(pub_buf);
		mbedtls_ecp_keypair_free(&kp);
		return JS_ThrowPlainError(ctx, "Failed to export public key: %d", ret);
	}

	// Export private key (scalar d)
	size_t priv_len = mbedtls_mpi_size(&kp.d);
	uint8_t *priv_buf = malloc(priv_len);
	if (!priv_buf) {
		free(pub_buf);
		mbedtls_ecp_keypair_free(&kp);
		return JS_EXCEPTION;
	}
	ret = mbedtls_mpi_write_binary(&kp.d, priv_buf, priv_len);
	if (ret != 0) {
		free(pub_buf);
		free(priv_buf);
		mbedtls_ecp_keypair_free(&kp);
		return JS_ThrowPlainError(ctx, "Failed to export private key: %d", ret);
	}

	mbedtls_ecp_keypair_free(&kp);

	JSValue result = JS_NewArray(ctx);
	JS_SetPropertyUint32(ctx, result, 0,
		JS_NewArrayBuffer(ctx, pub_buf, pub_len, free_array_buffer, NULL, false));
	JS_SetPropertyUint32(ctx, result, 1,
		JS_NewArrayBuffer(ctx, priv_buf, priv_len, free_array_buffer, NULL, false));
	return result;
}

// --- EC Import Private Key ---
// Creates a CryptoKey from private key scalar + public key point
static JSValue nx_crypto_key_new_ec_private(JSContext *ctx, JSValueConst this_val,
											 int argc, JSValueConst *argv) {
	// argv[0] = algorithm object, argv[1] = private key (scalar d), argv[2] = public key (uncompressed point)
	// argv[3] = extractable, argv[4] = usages
	nx_crypto_key_t *context = js_mallocz(ctx, sizeof(nx_crypto_key_t));
	if (!context) return JS_EXCEPTION;

	context->algorithm_cached = JS_UNDEFINED;
	context->usages_cached = JS_UNDEFINED;

	int extractable = JS_ToBool(ctx, argv[3]);
	context->extractable = extractable;

	// Parse usages
	uint32_t usages_size;
	JS_ToUint32(ctx, &usages_size, JS_GetPropertyStr(ctx, argv[4], "length"));
	for (uint32_t i = 0; i < usages_size; i++) {
		JSValue usage_val = JS_GetPropertyUint32(ctx, argv[4], i);
		const char *usage = JS_ToCString(ctx, usage_val);
		if (strcmp(usage, "sign") == 0) context->usages |= NX_CRYPTO_KEY_USAGE_SIGN;
		else if (strcmp(usage, "verify") == 0) context->usages |= NX_CRYPTO_KEY_USAGE_VERIFY;
		else if (strcmp(usage, "deriveBits") == 0) context->usages |= NX_CRYPTO_KEY_USAGE_DERIVE_BITS;
		else if (strcmp(usage, "deriveKey") == 0) context->usages |= NX_CRYPTO_KEY_USAGE_DERIVE_KEY;
		JS_FreeCString(ctx, usage);
	}

	context->type = NX_CRYPTO_KEY_TYPE_PRIVATE;

	JSValue algo_name_val = JS_GetPropertyStr(ctx, argv[0], "name");
	const char *algo_name = JS_ToCString(ctx, algo_name_val);
	context->algorithm = (strcmp(algo_name, "ECDSA") == 0)
		? NX_CRYPTO_KEY_ALGORITHM_ECDSA
		: NX_CRYPTO_KEY_ALGORITHM_ECDH;
	JS_FreeCString(ctx, algo_name);
	JS_FreeValue(ctx, algo_name_val);

	JSValue curve_val = JS_GetPropertyStr(ctx, argv[0], "namedCurve");
	const char *curve_name = JS_ToCString(ctx, curve_val);
	JS_FreeValue(ctx, curve_val);

	mbedtls_ecp_group_id grp_id;
	if (strcmp(curve_name, "P-256") == 0) {
		grp_id = MBEDTLS_ECP_DP_SECP256R1;
	} else if (strcmp(curve_name, "P-384") == 0) {
		grp_id = MBEDTLS_ECP_DP_SECP384R1;
	} else {
		JS_FreeCString(ctx, curve_name);
		js_free(ctx, context);
		return JS_ThrowTypeError(ctx, "Unsupported curve");
	}

	nx_crypto_key_ec_t *ec = js_mallocz(ctx, sizeof(nx_crypto_key_ec_t));
	if (!ec) {
		JS_FreeCString(ctx, curve_name);
		js_free(ctx, context);
		return JS_EXCEPTION;
	}
	mbedtls_ecp_keypair_init(&ec->keypair);
	strncpy(ec->curve_name, curve_name, sizeof(ec->curve_name) - 1);
	JS_FreeCString(ctx, curve_name);

	int ret = mbedtls_ecp_group_load(&ec->keypair.grp, grp_id);
	if (ret != 0) {
		mbedtls_ecp_keypair_free(&ec->keypair);
		js_free(ctx, ec);
		js_free(ctx, context);
		return JS_ThrowPlainError(ctx, "Failed to load EC group");
	}

	// Read private key scalar
	size_t priv_size;
	const uint8_t *priv_data = NX_GetBufferSource(ctx, &priv_size, argv[1]);
	ret = mbedtls_mpi_read_binary(&ec->keypair.d, priv_data, priv_size);
	if (ret != 0) {
		mbedtls_ecp_keypair_free(&ec->keypair);
		js_free(ctx, ec);
		js_free(ctx, context);
		return JS_ThrowPlainError(ctx, "Failed to read private key");
	}

	// Read public key point
	size_t pub_size;
	const uint8_t *pub_data = NX_GetBufferSource(ctx, &pub_size, argv[2]);
	ret = mbedtls_ecp_point_read_binary(&ec->keypair.grp, &ec->keypair.Q,
										 pub_data, pub_size);
	if (ret != 0) {
		mbedtls_ecp_keypair_free(&ec->keypair);
		js_free(ctx, ec);
		js_free(ctx, context);
		return JS_ThrowPlainError(ctx, "Failed to read public key");
	}

	context->handle = ec;

	// Store private key as raw_key_data for potential export
	context->raw_key_data = js_malloc(ctx, priv_size);
	if (context->raw_key_data) {
		memcpy(context->raw_key_data, priv_data, priv_size);
		context->raw_key_size = priv_size;
	}

	JSValue obj = JS_NewObjectClass(ctx, nx_crypto_key_class_id);
	if (JS_IsException(obj)) {
		mbedtls_ecp_keypair_free(&ec->keypair);
		js_free(ctx, ec);
		if (context->raw_key_data) js_free(ctx, context->raw_key_data);
		js_free(ctx, context);
		return obj;
	}

	JS_SetOpaque(obj, context);
	return obj;
}

// --- Derive Bits ---

typedef struct {
	int err;
	JSValue algorithm_val;
	JSValue key_val;
	nx_crypto_key_t *key;

	// ECDH
	JSValue public_key_val;
	nx_crypto_key_t *public_key;

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

	// ECDH deriveBits
	if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDH) {
		nx_crypto_key_ec_t *priv_ec = (nx_crypto_key_ec_t *)data->key->handle;
		nx_crypto_key_ec_t *pub_ec = (nx_crypto_key_ec_t *)data->public_key->handle;

		mbedtls_mpi shared;
		mbedtls_mpi_init(&shared);

		mbedtls_entropy_context entropy;
		mbedtls_ctr_drbg_context ctr_drbg;
		mbedtls_entropy_init(&entropy);
		mbedtls_ctr_drbg_init(&ctr_drbg);
		int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
		if (ret != 0) {
			mbedtls_ctr_drbg_free(&ctr_drbg);
			mbedtls_entropy_free(&entropy);
			mbedtls_mpi_free(&shared);
			data->err = ret;
			return;
		}

		ret = mbedtls_ecdh_compute_shared(&priv_ec->keypair.grp, &shared,
										   &pub_ec->keypair.Q, &priv_ec->keypair.d,
										   mbedtls_ctr_drbg_random, &ctr_drbg);
		mbedtls_ctr_drbg_free(&ctr_drbg);
		mbedtls_entropy_free(&entropy);

		if (ret != 0) {
			mbedtls_mpi_free(&shared);
			data->err = ret;
			return;
		}

		size_t coord_size = mbedtls_mpi_size(&priv_ec->keypair.grp.P);
		size_t out_bytes = data->length;
		if (out_bytes == 0) out_bytes = coord_size;

		data->result = calloc(1, out_bytes);
		if (!data->result) {
			mbedtls_mpi_free(&shared);
			data->err = ENOMEM;
			return;
		}

		uint8_t *full = calloc(1, coord_size);
		if (!full) {
			free(data->result);
			data->result = NULL;
			mbedtls_mpi_free(&shared);
			data->err = ENOMEM;
			return;
		}
		mbedtls_mpi_write_binary(&shared, full, coord_size);
		memcpy(data->result, full, out_bytes < coord_size ? out_bytes : coord_size);
		free(full);
		data->result_size = out_bytes;
		mbedtls_mpi_free(&shared);
		return;
	}

	// PBKDF2 / HKDF deriveBits
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
	if (!JS_IsUndefined(data->public_key_val)) {
		JS_FreeValue(ctx, data->public_key_val);
	}

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
	data->public_key_val = JS_UNDEFINED;

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

	if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDH) {
		// ECDH: get the public key from algorithm.public
		JSValue pub_val = JS_GetPropertyStr(ctx, argv[0], "public");
		data->public_key = JS_GetOpaque2(ctx, pub_val, nx_crypto_key_class_id);
		if (!data->public_key) {
			JS_FreeValue(ctx, pub_val);
			js_free(ctx, data);
			return JS_ThrowTypeError(ctx, "Missing public key in algorithm");
		}
		data->public_key_val = JS_DupValue(ctx, pub_val);
		JS_FreeValue(ctx, pub_val);
	} else {
		// PBKDF2 / HKDF: get hash from algorithm
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

// --- RSA Generate Key ---
// Returns array: [n, e, d, p, q, dp, dq, qi] as ArrayBuffers
typedef struct {
	int err;
	uint32_t modulus_length;
	uint32_t public_exponent;
	uint8_t *n; size_t n_len;
	uint8_t *e; size_t e_len;
	uint8_t *d; size_t d_len;
	uint8_t *p; size_t p_len;
	uint8_t *q; size_t q_len;
	uint8_t *dp; size_t dp_len;
	uint8_t *dq; size_t dq_len;
	uint8_t *qi; size_t qi_len;
} nx_crypto_generate_key_rsa_async_t;

static void nx_crypto_generate_key_rsa_do(nx_work_t *req) {
	nx_crypto_generate_key_rsa_async_t *data = (nx_crypto_generate_key_rsa_async_t *)req->data;

	mbedtls_rsa_context rsa;
	#if MBEDTLS_VERSION_MAJOR >= 3
	mbedtls_rsa_init(&rsa);
#else
	mbedtls_rsa_init(&rsa, MBEDTLS_RSA_PKCS_V21, MBEDTLS_MD_SHA256);
#endif

	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_entropy_init(&entropy);
	mbedtls_ctr_drbg_init(&ctr_drbg);
	int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
	if (ret != 0) {
		mbedtls_ctr_drbg_free(&ctr_drbg);
		mbedtls_entropy_free(&entropy);
		mbedtls_rsa_free(&rsa);
		data->err = ret;
		return;
	}

	ret = mbedtls_rsa_gen_key(&rsa, mbedtls_ctr_drbg_random, &ctr_drbg,
		data->modulus_length, data->public_exponent);
	mbedtls_ctr_drbg_free(&ctr_drbg);
	mbedtls_entropy_free(&entropy);

	if (ret != 0) {
		mbedtls_rsa_free(&rsa);
		data->err = ret;
		return;
	}

	mbedtls_mpi mpi_n, mpi_p, mpi_q, mpi_d, mpi_e, mpi_dp, mpi_dq, mpi_qi;
	mbedtls_mpi_init(&mpi_n); mbedtls_mpi_init(&mpi_p); mbedtls_mpi_init(&mpi_q);
	mbedtls_mpi_init(&mpi_d); mbedtls_mpi_init(&mpi_e);
	mbedtls_mpi_init(&mpi_dp); mbedtls_mpi_init(&mpi_dq); mbedtls_mpi_init(&mpi_qi);

	mbedtls_rsa_export(&rsa, &mpi_n, &mpi_p, &mpi_q, &mpi_d, &mpi_e);

	// Compute DP, DQ, QI
	mbedtls_mpi mpi_p1, mpi_q1;
	mbedtls_mpi_init(&mpi_p1); mbedtls_mpi_init(&mpi_q1);
	mbedtls_mpi_sub_int(&mpi_p1, &mpi_p, 1);
	mbedtls_mpi_sub_int(&mpi_q1, &mpi_q, 1);
	mbedtls_mpi_mod_mpi(&mpi_dp, &mpi_d, &mpi_p1);
	mbedtls_mpi_mod_mpi(&mpi_dq, &mpi_d, &mpi_q1);
	mbedtls_mpi_inv_mod(&mpi_qi, &mpi_q, &mpi_p);
	mbedtls_mpi_free(&mpi_p1); mbedtls_mpi_free(&mpi_q1);

	#define EXPORT_MPI(field) \
		data->field##_len = mbedtls_mpi_size(&mpi_##field); \
		data->field = malloc(data->field##_len); \
		if (!data->field) { data->err = ENOMEM; goto rsa_gen_cleanup; } \
		mbedtls_mpi_write_binary(&mpi_##field, data->field, data->field##_len);

	EXPORT_MPI(n); EXPORT_MPI(e); EXPORT_MPI(d);
	EXPORT_MPI(p); EXPORT_MPI(q);
	EXPORT_MPI(dp); EXPORT_MPI(dq); EXPORT_MPI(qi);
	#undef EXPORT_MPI

rsa_gen_cleanup:
	mbedtls_mpi_free(&mpi_n); mbedtls_mpi_free(&mpi_p); mbedtls_mpi_free(&mpi_q);
	mbedtls_mpi_free(&mpi_d); mbedtls_mpi_free(&mpi_e);
	mbedtls_mpi_free(&mpi_dp); mbedtls_mpi_free(&mpi_dq); mbedtls_mpi_free(&mpi_qi);
	mbedtls_rsa_free(&rsa);
	if (data->err) {
		free(data->n); free(data->e); free(data->d);
		free(data->p); free(data->q);
		free(data->dp); free(data->dq); free(data->qi);
	}
}

static JSValue nx_crypto_generate_key_rsa_cb(JSContext *ctx, nx_work_t *req) {
	nx_crypto_generate_key_rsa_async_t *data = (nx_crypto_generate_key_rsa_async_t *)req->data;

	if (data->err) {
		char err_buf[128];
		mbedtls_strerror(data->err, err_buf, sizeof(err_buf));
		JSValue err = JS_NewError(ctx);
		JS_DefinePropertyValueStr(ctx, err, "message",
			JS_NewString(ctx, err_buf),
			JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
		return JS_Throw(ctx, err);
	}

	JSValue result = JS_NewArray(ctx);
	#define SET_BUF(idx, field) \
		JS_SetPropertyUint32(ctx, result, idx, \
			JS_NewArrayBuffer(ctx, data->field, data->field##_len, free_array_buffer, NULL, false));
	SET_BUF(0, n); SET_BUF(1, e); SET_BUF(2, d);
	SET_BUF(3, p); SET_BUF(4, q);
	SET_BUF(5, dp); SET_BUF(6, dq); SET_BUF(7, qi);
	#undef SET_BUF
	return result;
}

static JSValue nx_crypto_generate_key_rsa(JSContext *ctx, JSValueConst this_val,
	int argc, JSValueConst *argv) {
	NX_INIT_WORK_T(nx_crypto_generate_key_rsa_async_t);

	u32 modulus_length, public_exponent;
	if (JS_ToUint32(ctx, &modulus_length, argv[0]) ||
		JS_ToUint32(ctx, &public_exponent, argv[1])) {
		js_free(ctx, data);
		return JS_EXCEPTION;
	}
	data->modulus_length = modulus_length;
	data->public_exponent = public_exponent;

	return nx_queue_async(ctx, req, nx_crypto_generate_key_rsa_do,
		nx_crypto_generate_key_rsa_cb);
}

// --- RSA Key New ---
// Creates a CryptoKey from RSA components
// argv[0] = algorithm name, argv[1] = hash name, argv[2] = type ("public"/"private")
// argv[3] = n, argv[4] = e, argv[5] = d (null for public), argv[6] = p, argv[7] = q
// argv[8] = extractable, argv[9] = usages
static JSValue nx_crypto_key_new_rsa(JSContext *ctx, JSValueConst this_val,
	int argc, JSValueConst *argv) {
	nx_crypto_key_t *context = js_mallocz(ctx, sizeof(nx_crypto_key_t));
	if (!context) return JS_EXCEPTION;

	context->algorithm_cached = JS_UNDEFINED;
	context->usages_cached = JS_UNDEFINED;

	const char *algo_name = JS_ToCString(ctx, argv[0]);
	if (!algo_name) { js_free(ctx, context); return JS_EXCEPTION; }

	if (strcmp(algo_name, "RSA-OAEP") == 0)
		context->algorithm = NX_CRYPTO_KEY_ALGORITHM_RSA_OAEP;
	else if (strcmp(algo_name, "RSA-PSS") == 0)
		context->algorithm = NX_CRYPTO_KEY_ALGORITHM_RSA_PSS;
	else if (strcmp(algo_name, "RSASSA-PKCS1-v1_5") == 0)
		context->algorithm = NX_CRYPTO_KEY_ALGORITHM_RSASSA_PKCS1_V1_5;
	else {
		JS_FreeCString(ctx, algo_name);
		js_free(ctx, context);
		return JS_ThrowTypeError(ctx, "Unsupported RSA algorithm");
	}
	JS_FreeCString(ctx, algo_name);

	const char *hash_name = JS_ToCString(ctx, argv[1]);
	if (!hash_name) { js_free(ctx, context); return JS_EXCEPTION; }

	const char *type_str = JS_ToCString(ctx, argv[2]);
	if (!type_str) { JS_FreeCString(ctx, hash_name); js_free(ctx, context); return JS_EXCEPTION; }
	if (strcmp(type_str, "public") == 0) {
		context->type = NX_CRYPTO_KEY_TYPE_PUBLIC;
	} else if (strcmp(type_str, "private") == 0) {
		context->type = NX_CRYPTO_KEY_TYPE_PRIVATE;
	} else {
		JS_FreeCString(ctx, type_str);
		JS_FreeCString(ctx, hash_name);
		js_free(ctx, context);
		return JS_ThrowTypeError(ctx, "Key type must be 'public' or 'private'");
	}
	JS_FreeCString(ctx, type_str);

	int extractable = JS_ToBool(ctx, argv[8]);
	context->extractable = extractable;

	// Parse usages
	uint32_t usages_size;
	JS_ToUint32(ctx, &usages_size, JS_GetPropertyStr(ctx, argv[9], "length"));
	for (uint32_t i = 0; i < usages_size; i++) {
		JSValue usage_val = JS_GetPropertyUint32(ctx, argv[9], i);
		if (!JS_IsString(usage_val)) {
			JS_FreeValue(ctx, usage_val);
			JS_FreeCString(ctx, hash_name);
			js_free(ctx, context);
			return JS_ThrowTypeError(ctx, "Expected string for usage");
		}
		const char *usage = JS_ToCString(ctx, usage_val);
		JS_FreeValue(ctx, usage_val);
		if (!usage) {
			JS_FreeCString(ctx, hash_name);
			js_free(ctx, context);
			return JS_EXCEPTION;
		}
		if (strcmp(usage, "encrypt") == 0) context->usages |= NX_CRYPTO_KEY_USAGE_ENCRYPT;
		else if (strcmp(usage, "decrypt") == 0) context->usages |= NX_CRYPTO_KEY_USAGE_DECRYPT;
		else if (strcmp(usage, "sign") == 0) context->usages |= NX_CRYPTO_KEY_USAGE_SIGN;
		else if (strcmp(usage, "verify") == 0) context->usages |= NX_CRYPTO_KEY_USAGE_VERIFY;
		else if (strcmp(usage, "wrapKey") == 0) context->usages |= NX_CRYPTO_KEY_USAGE_WRAP_KEY;
		else if (strcmp(usage, "unwrapKey") == 0) context->usages |= NX_CRYPTO_KEY_USAGE_UNWRAP_KEY;
		JS_FreeCString(ctx, usage);
	}

	nx_crypto_key_rsa_t *rsa = js_mallocz(ctx, sizeof(nx_crypto_key_rsa_t));
	if (!rsa) {
		JS_FreeCString(ctx, hash_name);
		js_free(ctx, context);
		return JS_EXCEPTION;
	}
	{
		int padding = (context->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSASSA_PKCS1_V1_5)
			? MBEDTLS_RSA_PKCS_V15 : MBEDTLS_RSA_PKCS_V21;
		mbedtls_md_type_t md = nx_crypto_get_md_type(hash_name);
#if MBEDTLS_VERSION_MAJOR >= 3
		mbedtls_rsa_init(&rsa->rsa);
		mbedtls_rsa_set_padding(&rsa->rsa, padding, md);
#else
		mbedtls_rsa_init(&rsa->rsa, padding, md);
#endif
	}
	strncpy(rsa->hash_name, hash_name, sizeof(rsa->hash_name) - 1);
	rsa->salt_length = -1;
	JS_FreeCString(ctx, hash_name);

	// Read N and E
	size_t n_size, e_size;
	const uint8_t *n_data = NX_GetBufferSource(ctx, &n_size, argv[3]);
	const uint8_t *e_data = NX_GetBufferSource(ctx, &e_size, argv[4]);
	if (!n_data || !e_data) {
		mbedtls_rsa_free(&rsa->rsa);
		js_free(ctx, rsa);
		js_free(ctx, context);
		return JS_EXCEPTION;
	}

	mbedtls_mpi N, E;
	mbedtls_mpi_init(&N); mbedtls_mpi_init(&E);
	mbedtls_mpi_read_binary(&N, n_data, n_size);
	mbedtls_mpi_read_binary(&E, e_data, e_size);

	if (context->type == NX_CRYPTO_KEY_TYPE_PRIVATE) {
		size_t d_size = 0, p_size = 0, q_size = 0;
		const uint8_t *d_data = NX_GetBufferSource(ctx, &d_size, argv[5]);
		const uint8_t *p_data = NX_GetBufferSource(ctx, &p_size, argv[6]);
		const uint8_t *q_data = NX_GetBufferSource(ctx, &q_size, argv[7]);

		mbedtls_mpi D, P, Q;
		mbedtls_mpi *pD = NULL, *pP = NULL, *pQ = NULL;
		mbedtls_mpi_init(&D); mbedtls_mpi_init(&P); mbedtls_mpi_init(&Q);
		if (d_data) { mbedtls_mpi_read_binary(&D, d_data, d_size); pD = &D; }
		if (p_data) { mbedtls_mpi_read_binary(&P, p_data, p_size); pP = &P; }
		if (q_data) { mbedtls_mpi_read_binary(&Q, q_data, q_size); pQ = &Q; }

		int ret = mbedtls_rsa_import(&rsa->rsa, &N, pP, pQ, pD, &E);
		mbedtls_mpi_free(&D); mbedtls_mpi_free(&P); mbedtls_mpi_free(&Q);
		if (ret == 0) ret = mbedtls_rsa_complete(&rsa->rsa);
		if (ret != 0) {
			mbedtls_mpi_free(&N); mbedtls_mpi_free(&E);
			mbedtls_rsa_free(&rsa->rsa);
			js_free(ctx, rsa);
			js_free(ctx, context);
			return JS_ThrowPlainError(ctx, "Failed to import RSA private key: %d", ret);
		}
	} else {
		int ret = mbedtls_rsa_import(&rsa->rsa, &N, NULL, NULL, NULL, &E);
		if (ret == 0) ret = mbedtls_rsa_complete(&rsa->rsa);
		if (ret != 0) {
			mbedtls_mpi_free(&N); mbedtls_mpi_free(&E);
			mbedtls_rsa_free(&rsa->rsa);
			js_free(ctx, rsa);
			js_free(ctx, context);
			return JS_ThrowPlainError(ctx, "Failed to import RSA public key: %d", ret);
		}
	}
	mbedtls_mpi_free(&N); mbedtls_mpi_free(&E);

	context->handle = rsa;

	JSValue obj = JS_NewObjectClass(ctx, nx_crypto_key_class_id);
	if (JS_IsException(obj)) {
		mbedtls_rsa_free(&rsa->rsa);
		js_free(ctx, rsa);
		js_free(ctx, context);
		return obj;
	}
	JS_SetOpaque(obj, context);
	return obj;
}

// --- RSA Export Key Components ---
// Returns [n, e, d, p, q, dp, dq, qi] for private keys, [n, e] for public
static JSValue nx_crypto_rsa_export_components(JSContext *ctx, JSValueConst this_val,
	int argc, JSValueConst *argv) {
	nx_crypto_key_t *key = JS_GetOpaque2(ctx, argv[0], nx_crypto_key_class_id);
	if (!key) return JS_EXCEPTION;

	if (!key->extractable)
		return JS_ThrowTypeError(ctx, "Key is not extractable");

	nx_crypto_key_rsa_t *rsa = (nx_crypto_key_rsa_t *)key->handle;
	if (!rsa)
		return JS_ThrowTypeError(ctx, "Not an RSA key");

	JSValue result = JS_NewArray(ctx);

	#define EXPORT_MPI_JS(idx, mpi_var) { \
		size_t len = mbedtls_mpi_size(&mpi_var); \
		uint8_t *buf = malloc(len); \
		if (buf) { \
			mbedtls_mpi_write_binary(&mpi_var, buf, len); \
			JS_SetPropertyUint32(ctx, result, idx, \
				JS_NewArrayBuffer(ctx, buf, len, free_array_buffer, NULL, false)); \
		} \
	}

	if (key->type == NX_CRYPTO_KEY_TYPE_PRIVATE) {
		mbedtls_mpi N, E, D, P, Q;
		mbedtls_mpi_init(&N); mbedtls_mpi_init(&E); mbedtls_mpi_init(&D);
		mbedtls_mpi_init(&P); mbedtls_mpi_init(&Q);
		mbedtls_rsa_export(&rsa->rsa, &N, &P, &Q, &D, &E);

		EXPORT_MPI_JS(0, N);
		EXPORT_MPI_JS(1, E);
		EXPORT_MPI_JS(2, D);
		EXPORT_MPI_JS(3, P);
		EXPORT_MPI_JS(4, Q);

		mbedtls_mpi DP, DQ, QI, P1, Q1;
		mbedtls_mpi_init(&DP); mbedtls_mpi_init(&DQ); mbedtls_mpi_init(&QI);
		mbedtls_mpi_init(&P1); mbedtls_mpi_init(&Q1);
		mbedtls_mpi_sub_int(&P1, &P, 1);
		mbedtls_mpi_sub_int(&Q1, &Q, 1);
		mbedtls_mpi_mod_mpi(&DP, &D, &P1);
		mbedtls_mpi_mod_mpi(&DQ, &D, &Q1);
		mbedtls_mpi_inv_mod(&QI, &Q, &P);
		EXPORT_MPI_JS(5, DP);
		EXPORT_MPI_JS(6, DQ);
		EXPORT_MPI_JS(7, QI);
		mbedtls_mpi_free(&DP); mbedtls_mpi_free(&DQ); mbedtls_mpi_free(&QI);
		mbedtls_mpi_free(&P1); mbedtls_mpi_free(&Q1);

		mbedtls_mpi_free(&N); mbedtls_mpi_free(&E); mbedtls_mpi_free(&D);
		mbedtls_mpi_free(&P); mbedtls_mpi_free(&Q);
	} else {
		// Public key — only export N and E
		mbedtls_mpi N, E;
		mbedtls_mpi_init(&N); mbedtls_mpi_init(&E);
		mbedtls_rsa_export(&rsa->rsa, &N, NULL, NULL, NULL, &E);

		EXPORT_MPI_JS(0, N);
		EXPORT_MPI_JS(1, E);

		mbedtls_mpi_free(&N); mbedtls_mpi_free(&E);
	}
	#undef EXPORT_MPI_JS

	return result;
}

// --- PKCS8 / SPKI Export ---
static JSValue nx_crypto_export_key_pkcs8(JSContext *ctx, JSValueConst this_val,
	int argc, JSValueConst *argv) {
	nx_crypto_key_t *key = JS_GetOpaque2(ctx, argv[0], nx_crypto_key_class_id);
	if (!key) return JS_EXCEPTION;
	if (!key->extractable)
		return JS_ThrowTypeError(ctx, "Key is not extractable");

	mbedtls_pk_context pk;
	mbedtls_pk_init(&pk);
	int ret;

	if (key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_OAEP ||
		key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_PSS ||
		key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSASSA_PKCS1_V1_5) {
		nx_crypto_key_rsa_t *rsa = (nx_crypto_key_rsa_t *)key->handle;
		ret = mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
		if (ret != 0) { mbedtls_pk_free(&pk); return JS_ThrowPlainError(ctx, "pk setup failed"); }

		mbedtls_mpi N, P, Q, D, E;
		mbedtls_mpi_init(&N); mbedtls_mpi_init(&P); mbedtls_mpi_init(&Q);
		mbedtls_mpi_init(&D); mbedtls_mpi_init(&E);
		mbedtls_rsa_export(&rsa->rsa, &N, &P, &Q, &D, &E);

		mbedtls_rsa_context *pk_rsa = mbedtls_pk_rsa(pk);
		mbedtls_rsa_import(pk_rsa, &N, &P, &Q, &D, &E);
		mbedtls_rsa_complete(pk_rsa);

		mbedtls_mpi_free(&N); mbedtls_mpi_free(&P); mbedtls_mpi_free(&Q);
		mbedtls_mpi_free(&D); mbedtls_mpi_free(&E);
	} else if (key->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDSA ||
			   key->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDH) {
		nx_crypto_key_ec_t *ec = (nx_crypto_key_ec_t *)key->handle;
		ret = mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
		if (ret != 0) { mbedtls_pk_free(&pk); return JS_ThrowPlainError(ctx, "pk setup failed"); }

		mbedtls_ecp_keypair *pk_ec = mbedtls_pk_ec(pk);
		mbedtls_ecp_group_load(&pk_ec->grp, ec->keypair.grp.id);
		mbedtls_mpi_copy(&pk_ec->d, &ec->keypair.d);
		mbedtls_ecp_copy(&pk_ec->Q, &ec->keypair.Q);
	} else {
		mbedtls_pk_free(&pk);
		return JS_ThrowTypeError(ctx, "Key type does not support PKCS8 export");
	}

	// Write DER
	uint8_t buf[4096];
	ret = mbedtls_pk_write_key_der(&pk, buf, sizeof(buf));
	mbedtls_pk_free(&pk);

	if (ret < 0)
		return JS_ThrowPlainError(ctx, "Failed to write PKCS8 DER: %d", ret);

	// mbedtls writes from the end of buf
	uint8_t *der_start = buf + sizeof(buf) - ret;
	uint8_t *result = malloc(ret);
	if (!result) return JS_EXCEPTION;
	memcpy(result, der_start, ret);
	return JS_NewArrayBuffer(ctx, result, ret, free_array_buffer, NULL, false);
}

static JSValue nx_crypto_export_key_spki(JSContext *ctx, JSValueConst this_val,
	int argc, JSValueConst *argv) {
	nx_crypto_key_t *key = JS_GetOpaque2(ctx, argv[0], nx_crypto_key_class_id);
	if (!key) return JS_EXCEPTION;
	if (!key->extractable)
		return JS_ThrowTypeError(ctx, "Key is not extractable");

	mbedtls_pk_context pk;
	mbedtls_pk_init(&pk);
	int ret;

	if (key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_OAEP ||
		key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_PSS ||
		key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSASSA_PKCS1_V1_5) {
		nx_crypto_key_rsa_t *rsa = (nx_crypto_key_rsa_t *)key->handle;
		ret = mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
		if (ret != 0) { mbedtls_pk_free(&pk); return JS_ThrowPlainError(ctx, "pk setup failed"); }

		mbedtls_mpi N, E;
		mbedtls_mpi_init(&N); mbedtls_mpi_init(&E);
		mbedtls_rsa_export(&rsa->rsa, &N, NULL, NULL, NULL, &E);

		mbedtls_rsa_context *pk_rsa = mbedtls_pk_rsa(pk);
		mbedtls_rsa_import(pk_rsa, &N, NULL, NULL, NULL, &E);
		mbedtls_rsa_complete(pk_rsa);

		mbedtls_mpi_free(&N); mbedtls_mpi_free(&E);
	} else if (key->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDSA ||
			   key->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDH) {
		nx_crypto_key_ec_t *ec = (nx_crypto_key_ec_t *)key->handle;
		ret = mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY));
		if (ret != 0) { mbedtls_pk_free(&pk); return JS_ThrowPlainError(ctx, "pk setup failed"); }

		mbedtls_ecp_keypair *pk_ec = mbedtls_pk_ec(pk);
		mbedtls_ecp_group_load(&pk_ec->grp, ec->keypair.grp.id);
		mbedtls_ecp_copy(&pk_ec->Q, &ec->keypair.Q);
	} else {
		mbedtls_pk_free(&pk);
		return JS_ThrowTypeError(ctx, "Key type does not support SPKI export");
	}

	uint8_t buf[4096];
	ret = mbedtls_pk_write_pubkey_der(&pk, buf, sizeof(buf));
	mbedtls_pk_free(&pk);

	if (ret < 0)
		return JS_ThrowPlainError(ctx, "Failed to write SPKI DER: %d", ret);

	uint8_t *der_start = buf + sizeof(buf) - ret;
	uint8_t *result = malloc(ret);
	if (!result) return JS_EXCEPTION;
	memcpy(result, der_start, ret);
	return JS_NewArrayBuffer(ctx, result, ret, free_array_buffer, NULL, false);
}

// --- PKCS8 / SPKI Import ---
// Returns a CryptoKey opaque object
// argv[0] = format ("pkcs8"/"spki"), argv[1] = DER data, argv[2] = algorithm name,
// argv[3] = hash name, argv[4] = extractable, argv[5] = usages
// For EC: argv[3] = namedCurve
static JSValue nx_crypto_import_key_pkcs8_spki(JSContext *ctx, JSValueConst this_val,
	int argc, JSValueConst *argv) {
	const char *format = JS_ToCString(ctx, argv[0]);
	if (!format) return JS_EXCEPTION;

	size_t der_size;
	const uint8_t *der_data = NX_GetBufferSource(ctx, &der_size, argv[1]);
	if (!der_data) { JS_FreeCString(ctx, format); return JS_EXCEPTION; }

	const char *algo_name = JS_ToCString(ctx, argv[2]);
	if (!algo_name) { JS_FreeCString(ctx, format); return JS_EXCEPTION; }

	const char *param_name = JS_ToCString(ctx, argv[3]); // hash or namedCurve
	if (!param_name) { JS_FreeCString(ctx, format); JS_FreeCString(ctx, algo_name); return JS_EXCEPTION; }

	int extractable = JS_ToBool(ctx, argv[4]);

	mbedtls_pk_context pk;
	mbedtls_pk_init(&pk);
	int ret;

	if (strcmp(format, "pkcs8") == 0) {
#if MBEDTLS_VERSION_MAJOR >= 3
		mbedtls_entropy_context imp_entropy;
		mbedtls_ctr_drbg_context imp_ctr_drbg;
		mbedtls_entropy_init(&imp_entropy);
		mbedtls_ctr_drbg_init(&imp_ctr_drbg);
		ret = mbedtls_ctr_drbg_seed(&imp_ctr_drbg, mbedtls_entropy_func, &imp_entropy, NULL, 0);
		if (ret != 0) {
			mbedtls_ctr_drbg_free(&imp_ctr_drbg);
			mbedtls_entropy_free(&imp_entropy);
			mbedtls_pk_free(&pk);
			JS_FreeCString(ctx, format);
			JS_FreeCString(ctx, algo_name);
			JS_FreeCString(ctx, param_name);
			char err_buf[128];
			mbedtls_strerror(ret, err_buf, sizeof(err_buf));
			return JS_ThrowPlainError(ctx, "Failed to seed DRBG: %s", err_buf);
		}
		ret = mbedtls_pk_parse_key(&pk, der_data, der_size, NULL, 0,
			mbedtls_ctr_drbg_random, &imp_ctr_drbg);
		mbedtls_ctr_drbg_free(&imp_ctr_drbg);
		mbedtls_entropy_free(&imp_entropy);
#else
		ret = mbedtls_pk_parse_key(&pk, der_data, der_size, NULL, 0);
#endif
	} else {
		ret = mbedtls_pk_parse_public_key(&pk, der_data, der_size);
	}

	if (ret != 0) {
		mbedtls_pk_free(&pk);
		JS_FreeCString(ctx, format);
		JS_FreeCString(ctx, algo_name);
		JS_FreeCString(ctx, param_name);
		return JS_ThrowPlainError(ctx, "Failed to parse key: -0x%04x", -ret);
	}

	nx_crypto_key_t *context = js_mallocz(ctx, sizeof(nx_crypto_key_t));
	if (!context) {
		mbedtls_pk_free(&pk);
		JS_FreeCString(ctx, format);
		JS_FreeCString(ctx, algo_name);
		JS_FreeCString(ctx, param_name);
		return JS_EXCEPTION;
	}
	context->algorithm_cached = JS_UNDEFINED;
	context->usages_cached = JS_UNDEFINED;
	context->extractable = extractable;

	// Parse usages
	uint32_t usages_size;
	JS_ToUint32(ctx, &usages_size, JS_GetPropertyStr(ctx, argv[5], "length"));
	for (uint32_t i = 0; i < usages_size; i++) {
		JSValue usage_val = JS_GetPropertyUint32(ctx, argv[5], i);
		if (!JS_IsString(usage_val)) {
			JS_FreeValue(ctx, usage_val);
			mbedtls_pk_free(&pk);
			JS_FreeCString(ctx, format); JS_FreeCString(ctx, algo_name); JS_FreeCString(ctx, param_name);
			js_free(ctx, context);
			return JS_ThrowTypeError(ctx, "Expected string for usage");
		}
		const char *usage = JS_ToCString(ctx, usage_val);
		JS_FreeValue(ctx, usage_val);
		if (!usage) {
			mbedtls_pk_free(&pk);
			JS_FreeCString(ctx, format); JS_FreeCString(ctx, algo_name); JS_FreeCString(ctx, param_name);
			js_free(ctx, context);
			return JS_EXCEPTION;
		}
		if (strcmp(usage, "encrypt") == 0) context->usages |= NX_CRYPTO_KEY_USAGE_ENCRYPT;
		else if (strcmp(usage, "decrypt") == 0) context->usages |= NX_CRYPTO_KEY_USAGE_DECRYPT;
		else if (strcmp(usage, "sign") == 0) context->usages |= NX_CRYPTO_KEY_USAGE_SIGN;
		else if (strcmp(usage, "verify") == 0) context->usages |= NX_CRYPTO_KEY_USAGE_VERIFY;
		else if (strcmp(usage, "wrapKey") == 0) context->usages |= NX_CRYPTO_KEY_USAGE_WRAP_KEY;
		else if (strcmp(usage, "unwrapKey") == 0) context->usages |= NX_CRYPTO_KEY_USAGE_UNWRAP_KEY;
		else if (strcmp(usage, "deriveBits") == 0) context->usages |= NX_CRYPTO_KEY_USAGE_DERIVE_BITS;
		else if (strcmp(usage, "deriveKey") == 0) context->usages |= NX_CRYPTO_KEY_USAGE_DERIVE_KEY;
		JS_FreeCString(ctx, usage);
	}

	context->type = (strcmp(format, "pkcs8") == 0) ? NX_CRYPTO_KEY_TYPE_PRIVATE : NX_CRYPTO_KEY_TYPE_PUBLIC;

	mbedtls_pk_type_t pk_type = mbedtls_pk_get_type(&pk);

	if (pk_type == MBEDTLS_PK_RSA) {
		if (strcmp(algo_name, "RSA-OAEP") == 0)
			context->algorithm = NX_CRYPTO_KEY_ALGORITHM_RSA_OAEP;
		else if (strcmp(algo_name, "RSA-PSS") == 0)
			context->algorithm = NX_CRYPTO_KEY_ALGORITHM_RSA_PSS;
		else if (strcmp(algo_name, "RSASSA-PKCS1-v1_5") == 0)
			context->algorithm = NX_CRYPTO_KEY_ALGORITHM_RSASSA_PKCS1_V1_5;
		else {
			mbedtls_pk_free(&pk);
			JS_FreeCString(ctx, format); JS_FreeCString(ctx, algo_name); JS_FreeCString(ctx, param_name);
			js_free(ctx, context);
			return JS_ThrowTypeError(ctx, "Unsupported RSA algorithm for import");
		}

		nx_crypto_key_rsa_t *rsa = js_mallocz(ctx, sizeof(nx_crypto_key_rsa_t));
		if (!rsa) {
			mbedtls_pk_free(&pk);
			JS_FreeCString(ctx, format); JS_FreeCString(ctx, algo_name); JS_FreeCString(ctx, param_name);
			js_free(ctx, context);
			return JS_EXCEPTION;
		}
		{
			int padding = (context->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSASSA_PKCS1_V1_5)
				? MBEDTLS_RSA_PKCS_V15 : MBEDTLS_RSA_PKCS_V21;
			mbedtls_md_type_t md = nx_crypto_get_md_type(param_name);
#if MBEDTLS_VERSION_MAJOR >= 3
			mbedtls_rsa_init(&rsa->rsa);
			mbedtls_rsa_set_padding(&rsa->rsa, padding, md);
#else
			mbedtls_rsa_init(&rsa->rsa, padding, md);
#endif
		}
		strncpy(rsa->hash_name, param_name, sizeof(rsa->hash_name) - 1);
		rsa->salt_length = -1;

		// Copy RSA context from pk
		mbedtls_rsa_context *src_rsa = mbedtls_pk_rsa(pk);
		ret = mbedtls_rsa_copy(&rsa->rsa, src_rsa);
		if (ret != 0) {
			mbedtls_pk_free(&pk);
			mbedtls_rsa_free(&rsa->rsa);
			js_free(ctx, rsa);
			JS_FreeCString(ctx, format); JS_FreeCString(ctx, algo_name); JS_FreeCString(ctx, param_name);
			js_free(ctx, context);
			return JS_ThrowPlainError(ctx, "Failed to copy RSA key: %d", ret);
		}

		context->handle = rsa;
	} else if (pk_type == MBEDTLS_PK_ECKEY) {
		if (strcmp(algo_name, "ECDSA") == 0)
			context->algorithm = NX_CRYPTO_KEY_ALGORITHM_ECDSA;
		else if (strcmp(algo_name, "ECDH") == 0)
			context->algorithm = NX_CRYPTO_KEY_ALGORITHM_ECDH;
		else {
			mbedtls_pk_free(&pk);
			JS_FreeCString(ctx, format); JS_FreeCString(ctx, algo_name); JS_FreeCString(ctx, param_name);
			js_free(ctx, context);
			return JS_ThrowTypeError(ctx, "Unsupported EC algorithm for import");
		}

		nx_crypto_key_ec_t *ec = js_mallocz(ctx, sizeof(nx_crypto_key_ec_t));
		if (!ec) {
			mbedtls_pk_free(&pk);
			JS_FreeCString(ctx, format); JS_FreeCString(ctx, algo_name); JS_FreeCString(ctx, param_name);
			js_free(ctx, context);
			return JS_EXCEPTION;
		}
		mbedtls_ecp_keypair_init(&ec->keypair);
		strncpy(ec->curve_name, param_name, sizeof(ec->curve_name) - 1);

		mbedtls_ecp_keypair *src_ec = mbedtls_pk_ec(pk);
		mbedtls_ecp_group_load(&ec->keypair.grp, src_ec->grp.id);
		mbedtls_ecp_copy(&ec->keypair.Q, &src_ec->Q);
		if (context->type == NX_CRYPTO_KEY_TYPE_PRIVATE)
			mbedtls_mpi_copy(&ec->keypair.d, &src_ec->d);

		context->handle = ec;
	} else {
		mbedtls_pk_free(&pk);
		JS_FreeCString(ctx, format); JS_FreeCString(ctx, algo_name); JS_FreeCString(ctx, param_name);
		js_free(ctx, context);
		return JS_ThrowTypeError(ctx, "Unsupported key type in DER");
	}

	mbedtls_pk_free(&pk);
	JS_FreeCString(ctx, format);
	JS_FreeCString(ctx, algo_name);
	JS_FreeCString(ctx, param_name);

	JSValue obj = JS_NewObjectClass(ctx, nx_crypto_key_class_id);
	if (JS_IsException(obj)) {
		// Free embedded mbedtls structures before freeing handle
		if (context->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_OAEP ||
			context->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_PSS ||
			context->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSASSA_PKCS1_V1_5) {
			nx_crypto_key_rsa_t *rsa_h = context->handle;
			if (rsa_h) mbedtls_rsa_free(&rsa_h->rsa);
		} else if (context->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDSA ||
				   context->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDH) {
			nx_crypto_key_ec_t *ec_h = context->handle;
			if (ec_h) mbedtls_ecp_keypair_free(&ec_h->keypair);
		}
		if (context->handle) js_free(ctx, context->handle);
		js_free(ctx, context);
		return obj;
	}
	JS_SetOpaque(obj, context);
	return obj;
}

// --- EC Export Public Raw ---
// Returns uncompressed public point (04 || x || y) for any EC key (public or private)
static JSValue nx_crypto_ec_export_public_raw(JSContext *ctx, JSValueConst this_val,
	int argc, JSValueConst *argv) {
	nx_crypto_key_t *key = JS_GetOpaque2(ctx, argv[0], nx_crypto_key_class_id);
	if (!key) return JS_EXCEPTION;

	if (key->algorithm != NX_CRYPTO_KEY_ALGORITHM_ECDSA &&
		key->algorithm != NX_CRYPTO_KEY_ALGORITHM_ECDH) {
		return JS_ThrowTypeError(ctx, "Not an EC key");
	}

	if (!key->extractable) {
		return JS_ThrowTypeError(ctx, "Key is not extractable");
	}

	nx_crypto_key_ec_t *ec = (nx_crypto_key_ec_t *)key->handle;
	size_t coord_size = mbedtls_mpi_size(&ec->keypair.grp.P);
	size_t buf_size = 1 + 2 * coord_size;
	uint8_t *buf = malloc(buf_size);
	if (!buf) return JS_EXCEPTION;

	size_t olen = 0;
	int ret = mbedtls_ecp_point_write_binary(&ec->keypair.grp, &ec->keypair.Q,
		MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, buf, buf_size);
	if (ret != 0) {
		free(buf);
		return JS_ThrowPlainError(ctx, "Failed to export EC public point: %d", ret);
	}

	return JS_NewArrayBuffer(ctx, buf, olen, free_array_buffer, NULL, false);
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
	JS_CFUNC_DEF("cryptoGenerateKeyEc", 0, nx_crypto_generate_key_ec),
	JS_CFUNC_DEF("cryptoKeyNewEcPrivate", 0, nx_crypto_key_new_ec_private),
	JS_CFUNC_DEF("cryptoDeriveBits", 0, nx_crypto_derive_bits),
	JS_CFUNC_DEF("cryptoGenerateKeyRsa", 0, nx_crypto_generate_key_rsa),
	JS_CFUNC_DEF("cryptoKeyNewRsa", 0, nx_crypto_key_new_rsa),
	JS_CFUNC_DEF("cryptoRsaExportComponents", 0, nx_crypto_rsa_export_components),
	JS_CFUNC_DEF("cryptoExportKeyPkcs8", 0, nx_crypto_export_key_pkcs8),
	JS_CFUNC_DEF("cryptoExportKeySpki", 0, nx_crypto_export_key_spki),
	JS_CFUNC_DEF("cryptoImportKeyPkcs8Spki", 0, nx_crypto_import_key_pkcs8_spki),
	JS_CFUNC_DEF("cryptoEcExportPublicRaw", 0, nx_crypto_ec_export_public_raw),
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
