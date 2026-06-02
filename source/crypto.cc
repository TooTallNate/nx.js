#include "crypto.h"
#include "async.h"
#include "error.h"
#include "util.h"
#include "wrap.h"
#include <errno.h>
#include <mbedtls/asn1.h>
#include <mbedtls/asn1write.h>
#include <mbedtls/bignum.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/ecp.h>
#include <mbedtls/entropy.h>
#include <mbedtls/error.h>
#include <mbedtls/gcm.h>
#include <mbedtls/hkdf.h>
#include <mbedtls/md.h>
#include <mbedtls/oid.h>
#include <mbedtls/pk.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/rsa.h>
#include <mbedtls/sha512.h>
#include <mbedtls/version.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>

using namespace v8;

namespace {

// Forward declaration (defined below)
mbedtls_md_type_t nx_crypto_get_md_type(const char *hash_name);

// Create an ArrayBuffer that takes ownership of a malloc'd buffer (mirrors the
// old QuickJS `JS_NewArrayBuffer(..., free_array_buffer, ...)`). On return the
// buffer is owned by V8 and will be free()'d when the ArrayBuffer is collected.
Local<Value> nx_ab_take(Isolate *iso, void *buf, size_t size) {
	std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
	    buf, size, [](void *p, size_t, void *) { free(p); }, nullptr);
	return ArrayBuffer::New(iso, std::move(bs)).As<Value>();
}

// Unwrap a CryptoKey JS object to its native context.
nx_crypto_key_t *nx_get_crypto_key(Local<Value> obj) {
	return nx::Unwrap<nx_crypto_key_t>(obj);
}

// ------------------------------------------------------------------
// PKCS#7 padding helpers
// ------------------------------------------------------------------

void *pad_pkcs7(size_t block_size, const uint8_t *input, size_t input_len,
                size_t *out_size) {
	size_t padded_len = ((input_len / block_size) + 1) * block_size;
	void *output = malloc(padded_len);
	if (output) {
		memcpy(output, input, input_len);
		uint8_t pad_value = padded_len - input_len;
		memset((uint8_t *)output + input_len, pad_value, pad_value);
		*out_size = padded_len;
	}
	return output;
}

size_t unpad_pkcs7(size_t block_size, uint8_t *input, size_t input_len) {
	if (input_len == 0)
		return 0;
	uint8_t pad_value = input[input_len - 1];
	if (pad_value > block_size || pad_value == 0 || pad_value > input_len)
		return input_len;
	uint8_t bad = 0;
	for (size_t i = 0; i < pad_value; i++) {
		bad |= input[input_len - 1 - i] ^ pad_value;
	}
	if (bad != 0)
		return input_len;
	return input_len - pad_value;
}

mbedtls_md_type_t nx_crypto_get_md_type(const char *hash_name) {
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

// ------------------------------------------------------------------
// CryptoKey finalizer (GC). MUST NOT touch any V8 API except Global::Reset.
// ------------------------------------------------------------------

void finalizer_crypto_key(nx_crypto_key_t *context) {
	if (!context)
		return;
	context->algorithm_cached.Reset();
	context->usages_cached.Reset();
	if (context->algorithm == NX_CRYPTO_KEY_ALGORITHM_HMAC) {
		nx_crypto_key_hmac_t *hmac = (nx_crypto_key_hmac_t *)context->handle;
		if (hmac && hmac->key) {
			free(hmac->key);
		}
	}
	if (context->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDSA ||
	    context->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDH) {
		nx_crypto_key_ec_t *ec = (nx_crypto_key_ec_t *)context->handle;
		if (ec) {
			mbedtls_ecp_keypair_free(&ec->keypair);
		}
	}
	if (context->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_OAEP ||
	    context->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_PSS ||
	    context->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSASSA_PKCS1_V1_5) {
		nx_crypto_key_rsa_t *rsa = (nx_crypto_key_rsa_t *)context->handle;
		if (rsa) {
			mbedtls_rsa_free(&rsa->rsa);
		}
	}
	if (context->handle) {
		free(context->handle);
	}
	if (context->raw_key_data) {
		free(context->raw_key_data);
	}
	free(context);
}

// Build a CryptoKey JS object wrapping `context`, registering the finalizer.
Local<Object> nx_make_crypto_key(Isolate *iso, nx_crypto_key_t *context) {
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_crypto_key_t>(iso, obj, context, finalizer_crypto_key);
	return obj;
}

// ==================================================================
// digest()
// ==================================================================

enum nx_crypto_algorithm {
	NX_CRYPTO_SHA1,
	NX_CRYPTO_SHA256,
	NX_CRYPTO_SHA384,
	NX_CRYPTO_SHA512,
};

struct nx_crypto_digest_async_t {
	int err = 0;
	char *algorithm = nullptr; // owned copy
	Global<Value> data_val;
	uint8_t *data = nullptr;
	size_t size = 0;
	uint8_t *result = nullptr;
	size_t result_size = 0;
};

void nx_crypto_digest_do(nx_work_t *req) {
	nx_crypto_digest_async_t *data = (nx_crypto_digest_async_t *)req->data;
	int alg = -1;
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
	data->result = (uint8_t *)calloc(1, data->result_size);
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
	case NX_CRYPTO_SHA512: {
		mbedtls_sha512_context sha512_ctx;
		mbedtls_sha512_init(&sha512_ctx);
		mbedtls_sha512_starts(&sha512_ctx, alg == NX_CRYPTO_SHA384);
		mbedtls_sha512_update(&sha512_ctx, data->data, data->size);
		mbedtls_sha512_finish(&sha512_ctx, data->result);
		mbedtls_sha512_free(&sha512_ctx);
		break;
	}
	}
}

MaybeLocal<Value> nx_crypto_digest_cb(Isolate *iso, nx_work_t *req) {
	nx_crypto_digest_async_t *data = (nx_crypto_digest_async_t *)req->data;
	free(data->algorithm);
	data->algorithm = nullptr;
	data->data_val.Reset();

	if (data->err) {
		nx_throw(iso, strerror(data->err));
		return MaybeLocal<Value>();
	}

	uint8_t *result = data->result;
	size_t result_size = data->result_size;
	data->result = nullptr;
	return nx_ab_take(iso, result, result_size);
}

void nx_crypto_digest(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	NX_INIT_WORK_T_CPP(nx_crypto_digest_async_t);

	String::Utf8Value algorithm(iso, info[0]);
	if (!*algorithm) {
		req->data_dtor(req->data);
		delete req;
		nx_throw(iso, "expected string algorithm");
		return;
	}
	data->algorithm = strdup(*algorithm);

	data->data = NX_GetBufferSource(iso, &data->size, info[1]);
	if (!data->data) {
		req->data_dtor(req->data);
		delete req;
		nx_throw(iso, "expected ArrayBuffer");
		return;
	}
	data->data_val.Reset(iso, info[1]);
	info.GetReturnValue().Set(
	    nx_queue_async(iso, req, nx_crypto_digest_do, nx_crypto_digest_cb));
}

// ==================================================================
// encrypt() / decrypt() shared async payload + AES param structs
// ==================================================================

struct nx_crypto_encrypt_async_t {
	int err = 0;
	Global<Value> algorithm_val;
	void *algorithm_params = nullptr;
	Global<Value> key_val;
	nx_crypto_key_t *key = nullptr;
	Global<Value> data_val;
	uint8_t *data = nullptr;
	size_t data_size = 0;
	void *result = nullptr;
	size_t result_size = 0;
};

struct nx_crypto_aes_cbc_params_t {
	uint8_t *iv;
};

struct nx_crypto_aes_ctr_params_t {
	uint8_t *ctr;
};

struct nx_crypto_aes_xts_params_t {
	u64 sector;
	size_t sector_size;
	bool is_nintendo;
};

struct nx_crypto_aes_gcm_params_t {
	uint8_t *iv;
	size_t iv_size;
	uint8_t *additional_data;
	size_t additional_data_size;
	size_t tag_length;
};

void nx_crypto_encrypt_do(nx_work_t *req) {
	nx_crypto_encrypt_async_t *data = (nx_crypto_encrypt_async_t *)req->data;

	if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_CBC) {
		nx_crypto_key_aes_t *aes = (nx_crypto_key_aes_t *)data->key->handle;
		nx_crypto_aes_cbc_params_t *cbc_params =
		    (nx_crypto_aes_cbc_params_t *)data->algorithm_params;

		data->result = (uint8_t *)pad_pkcs7(AES_BLOCK_SIZE, data->data,
		                                    data->data_size,
		                                    &data->result_size);
		if (!data->result) {
			data->err = ENOMEM;
			return;
		}

		if (aes->key_length == 16) {
			Aes128CbcContext ctx_copy = aes->encrypt.cbc_128;
			aes128CbcContextResetIv(&ctx_copy, cbc_params->iv);
			aes128CbcEncrypt(&ctx_copy, data->result, data->result,
			                 data->result_size);
		} else if (aes->key_length == 24) {
			Aes192CbcContext ctx_copy = aes->encrypt.cbc_192;
			aes192CbcContextResetIv(&ctx_copy, cbc_params->iv);
			aes192CbcEncrypt(&ctx_copy, data->result, data->result,
			                 data->result_size);
		} else if (aes->key_length == 32) {
			Aes256CbcContext ctx_copy = aes->encrypt.cbc_256;
			aes256CbcContextResetIv(&ctx_copy, cbc_params->iv);
			aes256CbcEncrypt(&ctx_copy, data->result, data->result,
			                 data->result_size);
		}
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_CTR) {
		nx_crypto_key_aes_t *aes = (nx_crypto_key_aes_t *)data->key->handle;
		nx_crypto_aes_ctr_params_t *ctr_params =
		    (nx_crypto_aes_ctr_params_t *)data->algorithm_params;

		data->result_size = data->data_size;
		data->result = (uint8_t *)malloc(data->result_size);
		if (!data->result) {
			data->err = ENOMEM;
			return;
		}

		if (aes->key_length == 16) {
			Aes128CtrContext ctx_copy = aes->decrypt.ctr_128;
			aes128CtrContextResetCtr(&ctx_copy, ctr_params->ctr);
			aes128CtrCrypt(&ctx_copy, data->result, data->data,
			               data->result_size);
		} else if (aes->key_length == 24) {
			Aes192CtrContext ctx_copy = aes->decrypt.ctr_192;
			aes192CtrContextResetCtr(&ctx_copy, ctr_params->ctr);
			aes192CtrCrypt(&ctx_copy, data->result, data->data,
			               data->result_size);
		} else if (aes->key_length == 32) {
			Aes256CtrContext ctx_copy = aes->decrypt.ctr_256;
			aes256CtrContextResetCtr(&ctx_copy, ctr_params->ctr);
			aes256CtrCrypt(&ctx_copy, data->result, data->data,
			               data->result_size);
		}
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_GCM) {
		nx_crypto_aes_gcm_params_t *gcm_params =
		    (nx_crypto_aes_gcm_params_t *)data->algorithm_params;

		size_t tag_len = gcm_params->tag_length;
		data->result_size = data->data_size + tag_len;
		data->result = (uint8_t *)malloc(data->result_size);
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
			data->result = nullptr;
			data->err = ENOTSUP;
			return;
		}

		ret = mbedtls_gcm_crypt_and_tag(
		    &gcm, MBEDTLS_GCM_ENCRYPT, data->data_size, gcm_params->iv,
		    gcm_params->iv_size, gcm_params->additional_data,
		    gcm_params->additional_data_size, data->data, (uint8_t *)data->result,
		    tag_len, (unsigned char *)data->result + data->data_size);
		mbedtls_gcm_free(&gcm);

		if (ret != 0) {
			free(data->result);
			data->result = nullptr;
			data->err = ENOTSUP;
			return;
		}
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_XTS) {
		nx_crypto_key_aes_t *aes = (nx_crypto_key_aes_t *)data->key->handle;
		nx_crypto_aes_xts_params_t *xts_params =
		    (nx_crypto_aes_xts_params_t *)data->algorithm_params;

		data->result = (uint8_t *)malloc(data->data_size);
		if (!data->result) {
			data->err = ENOMEM;
			return;
		}

		if (aes->key_length == 32) {
			Aes128XtsContext ctx_copy = aes->encrypt.xts_128;
			void *dst = data->result;
			void *src = data->data;
			u64 sector = xts_params->sector;
			for (size_t i = 0; i < data->data_size;
			     i += xts_params->sector_size) {
				aes128XtsContextResetSector(&ctx_copy, sector++,
				                            xts_params->is_nintendo);
				data->result_size += aes128XtsEncrypt(
				    &ctx_copy, dst, src, xts_params->sector_size);
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
		data->result = (uint8_t *)malloc(data->result_size);
		if (!data->result) {
			data->err = ENOMEM;
			return;
		}

		mbedtls_md_type_t md_type = nx_crypto_get_md_type(rsa->hash_name);

		mbedtls_entropy_context oaep_entropy;
		mbedtls_ctr_drbg_context oaep_ctr_drbg;
		mbedtls_entropy_init(&oaep_entropy);
		mbedtls_ctr_drbg_init(&oaep_ctr_drbg);
		int ret = mbedtls_ctr_drbg_seed(&oaep_ctr_drbg, mbedtls_entropy_func,
		                                &oaep_entropy, NULL, 0);
		if (ret != 0) {
			mbedtls_ctr_drbg_free(&oaep_ctr_drbg);
			mbedtls_entropy_free(&oaep_entropy);
			free(data->result);
			data->result = nullptr;
			data->err = ret;
			return;
		}

		mbedtls_rsa_set_padding(&rsa->rsa, MBEDTLS_RSA_PKCS_V21, md_type);

		uint8_t *label = NULL;
		size_t label_len = 0;
		if (data->algorithm_params) {
			nx_crypto_aes_gcm_params_t *lp =
			    (nx_crypto_aes_gcm_params_t *)data->algorithm_params;
			label = lp->iv;
			label_len = lp->iv_size;
		}

#if MBEDTLS_VERSION_MAJOR >= 3
		ret = mbedtls_rsa_rsaes_oaep_encrypt(
		    &rsa->rsa, mbedtls_ctr_drbg_random, &oaep_ctr_drbg,
		    (const unsigned char *)label, label_len, data->data_size,
		    data->data, (uint8_t *)data->result);
#else
		ret = mbedtls_rsa_rsaes_oaep_encrypt(
		    &rsa->rsa, mbedtls_ctr_drbg_random, &oaep_ctr_drbg,
		    MBEDTLS_RSA_PUBLIC, (const unsigned char *)label, label_len,
		    data->data_size, data->data, (uint8_t *)data->result);
#endif

		mbedtls_ctr_drbg_free(&oaep_ctr_drbg);
		mbedtls_entropy_free(&oaep_entropy);

		if (ret != 0) {
			free(data->result);
			data->result = nullptr;
			data->err = ENOTSUP;
		}
	}
}

MaybeLocal<Value> nx_crypto_encrypt_cb(Isolate *iso, nx_work_t *req) {
	nx_crypto_encrypt_async_t *data = (nx_crypto_encrypt_async_t *)req->data;
	if (data->algorithm_params) {
		free(data->algorithm_params);
		data->algorithm_params = nullptr;
	}
	data->algorithm_val.Reset();
	data->key_val.Reset();
	data->data_val.Reset();

	if (data->err) {
		nx_throw(iso, strerror(data->err));
		return MaybeLocal<Value>();
	}

	void *result = data->result;
	size_t result_size = data->result_size;
	data->result = nullptr;
	return nx_ab_take(iso, result, result_size);
}

// Parse algorithm params for AES/RSA-OAEP encrypt+decrypt. Returns true on
// success; on failure throws + returns false (caller must clean up req/data).
bool nx_crypto_parse_cipher_params(Isolate *iso, Local<Value> algorithm,
                                   nx_crypto_encrypt_async_t *data) {
	Local<Context> context = iso->GetCurrentContext();
	Local<Object> algo =
	    algorithm->IsObject() ? algorithm.As<Object>() : Local<Object>();

	if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_CBC) {
		nx_crypto_aes_cbc_params_t *cbc_params =
		    (nx_crypto_aes_cbc_params_t *)calloc(
		        1, sizeof(nx_crypto_aes_cbc_params_t));
		if (!cbc_params) {
			nx_throw(iso, "out of memory");
			return false;
		}
		size_t iv_size = 0;
		Local<Value> iv_val;
		if (!algo.IsEmpty() &&
		    algo->Get(context, nx_str(iso, "iv")).ToLocal(&iv_val)) {
			cbc_params->iv = NX_GetBufferSource(iso, &iv_size, iv_val);
		}
		if (!cbc_params->iv) {
			free(cbc_params);
			nx_throw(iso, "expected iv");
			return false;
		}
		if (iv_size != 16) {
			free(cbc_params);
			nx_throw(iso, "Initialization vector must be 16 bytes");
			return false;
		}
		data->algorithm_params = cbc_params;
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_CTR) {
		nx_crypto_aes_ctr_params_t *ctr_params =
		    (nx_crypto_aes_ctr_params_t *)calloc(
		        1, sizeof(nx_crypto_aes_ctr_params_t));
		if (!ctr_params) {
			nx_throw(iso, "out of memory");
			return false;
		}
		size_t ctr_size = 0;
		Local<Value> counter_val;
		if (!algo.IsEmpty() &&
		    algo->Get(context, nx_str(iso, "counter")).ToLocal(&counter_val)) {
			ctr_params->ctr = NX_GetBufferSource(iso, &ctr_size, counter_val);
		}
		if (!ctr_params->ctr) {
			free(ctr_params);
			nx_throw(iso, "expected counter");
			return false;
		}
		if (ctr_size != 16) {
			free(ctr_params);
			nx_throw(iso, "Counter must be 16 bytes");
			return false;
		}
		data->algorithm_params = ctr_params;
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_GCM) {
		nx_crypto_aes_gcm_params_t *gcm_params =
		    (nx_crypto_aes_gcm_params_t *)calloc(
		        1, sizeof(nx_crypto_aes_gcm_params_t));
		if (!gcm_params) {
			nx_throw(iso, "out of memory");
			return false;
		}
		Local<Value> iv_val;
		if (!algo.IsEmpty() &&
		    algo->Get(context, nx_str(iso, "iv")).ToLocal(&iv_val)) {
			gcm_params->iv =
			    NX_GetBufferSource(iso, &gcm_params->iv_size, iv_val);
		}
		if (!gcm_params->iv) {
			free(gcm_params);
			nx_throw(iso, "expected iv");
			return false;
		}
		Local<Value> ad_val;
		if (!algo.IsEmpty() &&
		    algo->Get(context, nx_str(iso, "additionalData")).ToLocal(&ad_val) &&
		    !ad_val->IsUndefined() && !ad_val->IsNull()) {
			gcm_params->additional_data = NX_GetBufferSource(
			    iso, &gcm_params->additional_data_size, ad_val);
		}
		Local<Value> tag_val;
		if (!algo.IsEmpty() &&
		    algo->Get(context, nx_str(iso, "tagLength")).ToLocal(&tag_val) &&
		    !tag_val->IsUndefined() && !tag_val->IsNull()) {
			uint32_t tag_bits = 0;
			tag_val->Uint32Value(context).To(&tag_bits);
			gcm_params->tag_length = tag_bits / 8;
		} else {
			gcm_params->tag_length = 16;
		}
		data->algorithm_params = gcm_params;
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_XTS) {
		nx_crypto_aes_xts_params_t *xts_params =
		    (nx_crypto_aes_xts_params_t *)calloc(
		        1, sizeof(nx_crypto_aes_xts_params_t));
		if (!xts_params) {
			nx_throw(iso, "out of memory");
			return false;
		}
		bool is_nintendo = false;
		uint32_t sector = 0, sector_size = 0;
		Local<Value> v;
		if (!algo.IsEmpty() &&
		    algo->Get(context, nx_str(iso, "isNintendo")).ToLocal(&v)) {
			is_nintendo = v->BooleanValue(iso);
		}
		if (!algo.IsEmpty() &&
		    algo->Get(context, nx_str(iso, "sector")).ToLocal(&v)) {
			v->Uint32Value(context).To(&sector);
		}
		if (!algo.IsEmpty() &&
		    algo->Get(context, nx_str(iso, "sectorSize")).ToLocal(&v)) {
			v->Uint32Value(context).To(&sector_size);
		}
		xts_params->is_nintendo = is_nintendo;
		xts_params->sector = sector;
		xts_params->sector_size = sector_size;
		data->algorithm_params = xts_params;
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_OAEP) {
		Local<Value> label_val;
		if (!algo.IsEmpty() &&
		    algo->Get(context, nx_str(iso, "label")).ToLocal(&label_val) &&
		    !label_val->IsUndefined() && !label_val->IsNull()) {
			nx_crypto_aes_gcm_params_t *lp =
			    (nx_crypto_aes_gcm_params_t *)calloc(
			        1, sizeof(nx_crypto_aes_gcm_params_t));
			if (!lp) {
				nx_throw(iso, "out of memory");
				return false;
			}
			lp->iv = NX_GetBufferSource(iso, &lp->iv_size, label_val);
			data->algorithm_params = lp;
		}
	}
	return true;
}

void nx_crypto_encrypt(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	NX_INIT_WORK_T_CPP(nx_crypto_encrypt_async_t);

	data->key = nx_get_crypto_key(info[1]);
	if (!data->key) {
		req->data_dtor(req->data);
		delete req;
		nx_throw(iso, "invalid key");
		return;
	}
	if (!(data->key->usages &
	      (NX_CRYPTO_KEY_USAGE_ENCRYPT | NX_CRYPTO_KEY_USAGE_WRAP_KEY))) {
		req->data_dtor(req->data);
		delete req;
		nx_throw(iso, "Key does not support the 'encrypt' operation");
		return;
	}
	data->data = NX_GetBufferSource(iso, &data->data_size, info[2]);
	if (!data->data) {
		req->data_dtor(req->data);
		delete req;
		nx_throw(iso, "expected ArrayBuffer");
		return;
	}
	if (!nx_crypto_parse_cipher_params(iso, info[0], data)) {
		req->data_dtor(req->data);
		delete req;
		return;
	}
	data->algorithm_val.Reset(iso, info[0]);
	data->key_val.Reset(iso, info[1]);
	data->data_val.Reset(iso, info[2]);
	info.GetReturnValue().Set(
	    nx_queue_async(iso, req, nx_crypto_encrypt_do, nx_crypto_encrypt_cb));
}

// ==================================================================
// decrypt()
// ==================================================================

void nx_crypto_decrypt_do(nx_work_t *req) {
	nx_crypto_encrypt_async_t *data = (nx_crypto_encrypt_async_t *)req->data;

	if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_CBC) {
		nx_crypto_key_aes_t *aes = (nx_crypto_key_aes_t *)data->key->handle;
		nx_crypto_aes_cbc_params_t *cbc_params =
		    (nx_crypto_aes_cbc_params_t *)data->algorithm_params;

		data->result = (uint8_t *)calloc(data->data_size, 1);
		if (!data->result) {
			data->err = ENOMEM;
			return;
		}

		if (aes->key_length == 16) {
			Aes128CbcContext ctx_copy = aes->decrypt.cbc_128;
			aes128CbcContextResetIv(&ctx_copy, cbc_params->iv);
			aes128CbcDecrypt(&ctx_copy, data->result, data->data,
			                 data->data_size);
		} else if (aes->key_length == 24) {
			Aes192CbcContext ctx_copy = aes->decrypt.cbc_192;
			aes192CbcContextResetIv(&ctx_copy, cbc_params->iv);
			aes192CbcDecrypt(&ctx_copy, data->result, data->data,
			                 data->data_size);
		} else if (aes->key_length == 32) {
			Aes256CbcContext ctx_copy = aes->decrypt.cbc_256;
			aes256CbcContextResetIv(&ctx_copy, cbc_params->iv);
			aes256CbcDecrypt(&ctx_copy, data->result, data->data,
			                 data->data_size);
		}

		data->result_size =
		    unpad_pkcs7(AES_BLOCK_SIZE, (uint8_t *)data->result,
		                data->data_size);
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_CTR) {
		nx_crypto_key_aes_t *aes = (nx_crypto_key_aes_t *)data->key->handle;
		nx_crypto_aes_ctr_params_t *ctr_params =
		    (nx_crypto_aes_ctr_params_t *)data->algorithm_params;

		data->result_size = data->data_size;
		data->result = (uint8_t *)malloc(data->result_size);
		if (!data->result) {
			data->err = ENOMEM;
			return;
		}

		if (aes->key_length == 16) {
			Aes128CtrContext ctx_copy = aes->decrypt.ctr_128;
			aes128CtrContextResetCtr(&ctx_copy, ctr_params->ctr);
			aes128CtrCrypt(&ctx_copy, data->result, data->data,
			               data->result_size);
		} else if (aes->key_length == 24) {
			Aes192CtrContext ctx_copy = aes->decrypt.ctr_192;
			aes192CtrContextResetCtr(&ctx_copy, ctr_params->ctr);
			aes192CtrCrypt(&ctx_copy, data->result, data->data,
			               data->result_size);
		} else if (aes->key_length == 32) {
			Aes256CtrContext ctx_copy = aes->decrypt.ctr_256;
			aes256CtrContextResetCtr(&ctx_copy, ctr_params->ctr);
			aes256CtrCrypt(&ctx_copy, data->result, data->data,
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
		data->result = (uint8_t *)malloc(data->result_size);
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
			data->result = nullptr;
			data->err = ENOTSUP;
			return;
		}

		ret = mbedtls_gcm_auth_decrypt(
		    &gcm, ciphertext_size, gcm_params->iv, gcm_params->iv_size,
		    gcm_params->additional_data, gcm_params->additional_data_size,
		    (const unsigned char *)data->data + ciphertext_size, tag_len,
		    data->data, (uint8_t *)data->result);
		mbedtls_gcm_free(&gcm);

		if (ret != 0) {
			free(data->result);
			data->result = nullptr;
			data->err = EACCES;
			return;
		}
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_XTS) {
		nx_crypto_key_aes_t *aes = (nx_crypto_key_aes_t *)data->key->handle;
		nx_crypto_aes_xts_params_t *xts_params =
		    (nx_crypto_aes_xts_params_t *)data->algorithm_params;

		data->result = (uint8_t *)malloc(data->data_size);
		if (!data->result) {
			data->err = ENOMEM;
			return;
		}

		if (aes->key_length == 32) {
			Aes128XtsContext ctx_copy = aes->decrypt.xts_128;
			void *dst = data->result;
			void *src = data->data;
			uint64_t sector = xts_params->sector;
			for (size_t i = 0; i < data->data_size;
			     i += xts_params->sector_size) {
				aes128XtsContextResetSector(&ctx_copy, sector++,
				                            xts_params->is_nintendo);
				data->result_size += aes128XtsDecrypt(
				    &ctx_copy, dst, src, xts_params->sector_size);
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
		int ret = mbedtls_ctr_drbg_seed(&dec_ctr_drbg, mbedtls_entropy_func,
		                                &dec_entropy, NULL, 0);
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
			nx_crypto_aes_gcm_params_t *lp =
			    (nx_crypto_aes_gcm_params_t *)data->algorithm_params;
			label = lp->iv;
			label_len = lp->iv_size;
		}

		uint8_t *output = (uint8_t *)malloc(rsa_len);
		if (!output) {
			mbedtls_ctr_drbg_free(&dec_ctr_drbg);
			mbedtls_entropy_free(&dec_entropy);
			data->err = ENOMEM;
			return;
		}

		size_t olen = 0;
#if MBEDTLS_VERSION_MAJOR >= 3
		ret = mbedtls_rsa_rsaes_oaep_decrypt(
		    &rsa->rsa, mbedtls_ctr_drbg_random, &dec_ctr_drbg,
		    (const unsigned char *)label, label_len, &olen, data->data, output,
		    rsa_len);
#else
		ret = mbedtls_rsa_rsaes_oaep_decrypt(
		    &rsa->rsa, mbedtls_ctr_drbg_random, &dec_ctr_drbg,
		    MBEDTLS_RSA_PRIVATE, (const unsigned char *)label, label_len, &olen,
		    data->data, output, rsa_len);
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

void nx_crypto_decrypt(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	NX_INIT_WORK_T_CPP(nx_crypto_encrypt_async_t);

	data->key = nx_get_crypto_key(info[1]);
	if (!data->key) {
		req->data_dtor(req->data);
		delete req;
		nx_throw(iso, "invalid key");
		return;
	}
	if (!(data->key->usages &
	      (NX_CRYPTO_KEY_USAGE_DECRYPT | NX_CRYPTO_KEY_USAGE_UNWRAP_KEY))) {
		req->data_dtor(req->data);
		delete req;
		nx_throw(iso, "Key does not support the 'decrypt' operation");
		return;
	}
	data->data = NX_GetBufferSource(iso, &data->data_size, info[2]);
	if (!data->data) {
		req->data_dtor(req->data);
		delete req;
		nx_throw(iso, "expected ArrayBuffer");
		return;
	}
	if (!nx_crypto_parse_cipher_params(iso, info[0], data)) {
		req->data_dtor(req->data);
		delete req;
		return;
	}
	data->algorithm_val.Reset(iso, info[0]);
	data->key_val.Reset(iso, info[1]);
	data->data_val.Reset(iso, info[2]);
	info.GetReturnValue().Set(
	    nx_queue_async(iso, req, nx_crypto_decrypt_do, nx_crypto_encrypt_cb));
}

// ==================================================================
// getRandomValues(), sha256Hex(), Crypto.prototype init
// ==================================================================

void nx_crypto_get_random_values(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	if (info.Length() < 1) {
		nx_throw(iso, "getRandomValues: 1 argument required");
		return;
	}
	size_t size = 0;
	void *buf = NX_GetBufferSource(iso, &size, info[0]);
	if (buf) {
		if (size > 65536) {
			// Throw a QuotaExceededError DOMException if available.
			Local<Object> global = context->Global();
			Local<Value> ctor_val;
			char msg[256];
			snprintf(msg, sizeof(msg),
			         "Failed to execute 'getRandomValues' on 'Crypto': The "
			         "ArrayBufferView's byte length (%zu) exceeds the number "
			         "of bytes of entropy available via this API (65536).",
			         size);
			if (global->Get(context, nx_str(iso, "DOMException"))
			        .ToLocal(&ctor_val) &&
			    ctor_val->IsFunction()) {
				Local<Function> ctor = ctor_val.As<Function>();
				Local<Value> args[2] = {nx_str(iso, msg),
				                        nx_str(iso, "QuotaExceededError")};
				Local<Value> err;
				if (ctor->NewInstance(context, 2, args).ToLocal(&err)) {
					iso->ThrowException(err);
					return;
				}
			}
			nx_throw(iso, msg);
			return;
		}
		randomGet(buf, size);
	}
	info.GetReturnValue().Set(info[0]);
}

void nx_crypto_sha256_hex(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value str(iso, info[0]);
	if (!*str) {
		nx_throw(iso, "expected string");
		return;
	}
	u8 digest[SHA256_HASH_SIZE];
	sha256CalculateHash(digest, *str, str.length());
	char hex[SHA256_HASH_SIZE * 2 + 1];
	for (int i = 0; i < SHA256_HASH_SIZE; i++) {
		snprintf(hex + i * 2, 3, "%02x", digest[i]);
	}
	info.GetReturnValue().Set(nx_str(iso, hex));
}

void nx_crypto_init(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	Local<Object> proto = info[0]
	                          .As<Object>()
	                          ->Get(context, nx_str(iso, "prototype"))
	                          .ToLocalChecked()
	                          .As<Object>();
	NX_DEF_FUNC(proto, "getRandomValues", nx_crypto_get_random_values, 1);
}

// ==================================================================
// CryptoKey accessors + cryptoKeyInit
// ==================================================================

void nx_crypto_key_get_type(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_crypto_key_t *context = nx_get_crypto_key(info.This());
	if (!context) {
		nx_throw(iso, "invalid CryptoKey");
		return;
	}
	const char *type;
	switch (context->type) {
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
	info.GetReturnValue().Set(nx_str(iso, type));
}

void nx_crypto_key_get_extractable(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_crypto_key_t *context = nx_get_crypto_key(info.This());
	if (!context) {
		nx_throw(iso, "invalid CryptoKey");
		return;
	}
	info.GetReturnValue().Set(Boolean::New(iso, context->extractable));
}

void nx_crypto_key_get_algorithm(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	nx_crypto_key_t *key = nx_get_crypto_key(info.This());
	if (!key) {
		nx_throw(iso, "invalid CryptoKey");
		return;
	}

	if (key->algorithm_cached.IsEmpty()) {
		Local<Object> obj = Object::New(iso);
		const char *name_val = "";

		switch (key->algorithm) {
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
			nx_crypto_key_hmac_t *hmac = (nx_crypto_key_hmac_t *)key->handle;
			Local<Object> hash_obj = Object::New(iso);
			hash_obj
			    ->Set(context, nx_str(iso, "name"),
			          nx_str(iso, hmac->hash_name))
			    .Check();
			obj->Set(context, nx_str(iso, "hash"), hash_obj).Check();
			obj->Set(context, nx_str(iso, "length"),
			         Integer::NewFromUnsigned(iso, hmac->key_length * 8))
			    .Check();
			break;
		}
		case NX_CRYPTO_KEY_ALGORITHM_ECDSA:
		case NX_CRYPTO_KEY_ALGORITHM_ECDH: {
			name_val = (key->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDSA)
			               ? "ECDSA"
			               : "ECDH";
			nx_crypto_key_ec_t *ec = (nx_crypto_key_ec_t *)key->handle;
			obj->Set(context, nx_str(iso, "namedCurve"),
			         nx_str(iso, ec->curve_name))
			    .Check();
			break;
		}
		case NX_CRYPTO_KEY_ALGORITHM_RSA_OAEP:
		case NX_CRYPTO_KEY_ALGORITHM_RSA_PSS:
		case NX_CRYPTO_KEY_ALGORITHM_RSASSA_PKCS1_V1_5: {
			name_val = (key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_OAEP)
			               ? "RSA-OAEP"
			               : (key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_PSS)
			                     ? "RSA-PSS"
			                     : "RSASSA-PKCS1-v1_5";
			nx_crypto_key_rsa_t *rsa = (nx_crypto_key_rsa_t *)key->handle;
			obj->Set(
			       context, nx_str(iso, "modulusLength"),
			       Integer::NewFromUnsigned(
			           iso, (uint32_t)(mbedtls_rsa_get_len(&rsa->rsa) * 8)))
			    .Check();
			Local<Object> hash_obj = Object::New(iso);
			hash_obj
			    ->Set(context, nx_str(iso, "name"), nx_str(iso, rsa->hash_name))
			    .Check();
			obj->Set(context, nx_str(iso, "hash"), hash_obj).Check();
			mbedtls_mpi E;
			mbedtls_mpi_init(&E);
			mbedtls_rsa_export(&rsa->rsa, NULL, NULL, NULL, NULL, &E);
			size_t pe_len = mbedtls_mpi_size(&E);
			uint8_t *pe_copy = (uint8_t *)malloc(pe_len);
			if (pe_copy) {
				mbedtls_mpi_write_binary(&E, pe_copy, pe_len);
				obj->Set(context, nx_str(iso, "publicExponent"),
				         nx_ab_take(iso, pe_copy, pe_len))
				    .Check();
			}
			mbedtls_mpi_free(&E);
			break;
		}
		default:
			break;
		}
		obj->Set(context, nx_str(iso, "name"), nx_str(iso, name_val)).Check();

		if (key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_CBC ||
		    key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_CTR ||
		    key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_XTS) {
			nx_crypto_key_aes_t *aes = (nx_crypto_key_aes_t *)key->handle;
			obj->Set(context, nx_str(iso, "length"),
			         Integer::NewFromUnsigned(iso, aes->key_length * 8))
			    .Check();
		} else if (key->algorithm == NX_CRYPTO_KEY_ALGORITHM_AES_GCM) {
			obj->Set(context, nx_str(iso, "length"),
			         Integer::NewFromUnsigned(
			             iso, (uint32_t)(key->raw_key_size * 8)))
			    .Check();
		}
		key->algorithm_cached.Reset(iso, obj);
	}

	info.GetReturnValue().Set(key->algorithm_cached.Get(iso));
}

void nx_crypto_key_get_usages(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	nx_crypto_key_t *key = nx_get_crypto_key(info.This());
	if (!key) {
		nx_throw(iso, "invalid CryptoKey");
		return;
	}

	if (key->usages_cached.IsEmpty()) {
		Local<Array> arr = Array::New(iso);
		uint32_t index = 0;
		auto push = [&](const char *s) {
			arr->Set(context, index++, nx_str(iso, s)).Check();
		};
		if (key->usages & NX_CRYPTO_KEY_USAGE_DECRYPT)
			push("decrypt");
		if (key->usages & NX_CRYPTO_KEY_USAGE_DERIVE_BITS)
			push("deriveBits");
		if (key->usages & NX_CRYPTO_KEY_USAGE_DERIVE_KEY)
			push("deriveKey");
		if (key->usages & NX_CRYPTO_KEY_USAGE_ENCRYPT)
			push("encrypt");
		if (key->usages & NX_CRYPTO_KEY_USAGE_SIGN)
			push("sign");
		if (key->usages & NX_CRYPTO_KEY_USAGE_UNWRAP_KEY)
			push("unwrapKey");
		if (key->usages & NX_CRYPTO_KEY_USAGE_VERIFY)
			push("verify");
		if (key->usages & NX_CRYPTO_KEY_USAGE_WRAP_KEY)
			push("wrapKey");
		key->usages_cached.Reset(iso, arr);
	}

	info.GetReturnValue().Set(key->usages_cached.Get(iso));
}

void nx_crypto_key_init(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	Local<Object> proto = info[0]
	                          .As<Object>()
	                          ->Get(context, nx_str(iso, "prototype"))
	                          .ToLocalChecked()
	                          .As<Object>();
	NX_DEF_GET(proto, "type", nx_crypto_key_get_type);
	NX_DEF_GET(proto, "extractable", nx_crypto_key_get_extractable);
	NX_DEF_GET(proto, "algorithm", nx_crypto_key_get_algorithm);
	NX_DEF_GET(proto, "usages", nx_crypto_key_get_usages);
}

// Parse a usages array (JS) into the bitmask. Returns false + throws on error.
bool nx_crypto_parse_usages(Isolate *iso, Local<Value> usages_val,
                            nx_crypto_key_usage *out, bool strict) {
	Local<Context> context = iso->GetCurrentContext();
	*out = (nx_crypto_key_usage)0;
	if (!usages_val->IsObject())
		return true;
	Local<Object> arr = usages_val.As<Object>();
	Local<Value> len_val;
	if (!arr->Get(context, nx_str(iso, "length")).ToLocal(&len_val))
		return true;
	uint32_t n = 0;
	len_val->Uint32Value(context).To(&n);
	uint32_t mask = 0;
	for (uint32_t i = 0; i < n; i++) {
		Local<Value> uv;
		if (!arr->Get(context, i).ToLocal(&uv))
			continue;
		if (!uv->IsString()) {
			if (strict) {
				nx_throw(iso, "Expected string for usage");
				return false;
			}
			continue;
		}
		String::Utf8Value usage(iso, uv);
		const char *u = *usage;
		if (!u)
			continue;
		if (strcmp(u, "decrypt") == 0)
			mask |= NX_CRYPTO_KEY_USAGE_DECRYPT;
		else if (strcmp(u, "deriveBits") == 0)
			mask |= NX_CRYPTO_KEY_USAGE_DERIVE_BITS;
		else if (strcmp(u, "deriveKey") == 0)
			mask |= NX_CRYPTO_KEY_USAGE_DERIVE_KEY;
		else if (strcmp(u, "encrypt") == 0)
			mask |= NX_CRYPTO_KEY_USAGE_ENCRYPT;
		else if (strcmp(u, "sign") == 0)
			mask |= NX_CRYPTO_KEY_USAGE_SIGN;
		else if (strcmp(u, "unwrapKey") == 0)
			mask |= NX_CRYPTO_KEY_USAGE_UNWRAP_KEY;
		else if (strcmp(u, "verify") == 0)
			mask |= NX_CRYPTO_KEY_USAGE_VERIFY;
		else if (strcmp(u, "wrapKey") == 0)
			mask |= NX_CRYPTO_KEY_USAGE_WRAP_KEY;
		else if (strict) {
			nx_throw(iso, "Invalid usage");
			return false;
		}
	}
	*out = (nx_crypto_key_usage)mask;
	return true;
}

// Read a "hash" field that may be a string or {name} object into `out` (sized
// buffer). Returns false on failure.
bool nx_crypto_read_hash_name(Isolate *iso, Local<Object> algo, char *out,
                              size_t out_size) {
	Local<Context> context = iso->GetCurrentContext();
	Local<Value> hash_val;
	if (!algo->Get(context, nx_str(iso, "hash")).ToLocal(&hash_val))
		return false;
	if (hash_val->IsString()) {
		String::Utf8Value s(iso, hash_val);
		if (!*s)
			return false;
		strncpy(out, *s, out_size - 1);
		out[out_size - 1] = 0;
		return true;
	}
	if (hash_val->IsObject()) {
		Local<Value> name_val;
		if (!hash_val.As<Object>()
		         ->Get(context, nx_str(iso, "name"))
		         .ToLocal(&name_val))
			return false;
		String::Utf8Value s(iso, name_val);
		if (!*s)
			return false;
		strncpy(out, *s, out_size - 1);
		out[out_size - 1] = 0;
		return true;
	}
	return false;
}

// ==================================================================
// cryptoKeyNew — import a "raw" symmetric / EC-public key
// argv[0]=algorithm obj, argv[1]=key data, argv[2]=extractable, argv[3]=usages
// ==================================================================

void nx_crypto_key_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();

	nx_crypto_key_t *kctx = (nx_crypto_key_t *)calloc(1, sizeof(nx_crypto_key_t));
	if (!kctx) {
		nx_throw(iso, "out of memory");
		return;
	}
	// Placement-new the Global members (calloc gave raw zero bytes).
	new (&kctx->algorithm_cached) Global<Value>();
	new (&kctx->usages_cached) Global<Value>();

	size_t key_size = 0;
	const uint8_t *key_data = NX_GetBufferSource(iso, &key_size, info[1]);
	if (!key_data) {
		free(kctx);
		nx_throw(iso, "expected key data");
		return;
	}

	kctx->extractable = info[2]->BooleanValue(iso);

	nx_crypto_key_usage usages;
	if (!nx_crypto_parse_usages(iso, info[3], &usages, true)) {
		free(kctx);
		return;
	}
	kctx->usages = usages;

	Local<Object> algo = info[0].As<Object>();
	Local<Value> name_val;
	if (!algo->Get(context, nx_str(iso, "name")).ToLocal(&name_val) ||
	    !name_val->IsString()) {
		free(kctx);
		nx_throw(iso, "Expected string for algorithm \"name\"");
		return;
	}
	String::Utf8Value algo_str(iso, name_val);
	const char *algo_name = *algo_str;

#define KEY_NEW_FAIL(msg)                                                      \
	do {                                                                       \
		if (kctx->raw_key_data)                                                \
			free(kctx->raw_key_data);                                          \
		free(kctx);                                                            \
		nx_throw(iso, msg);                                                    \
		return;                                                                \
	} while (0)

	// Store raw key data for export.
	kctx->raw_key_data = (uint8_t *)malloc(key_size);
	if (!kctx->raw_key_data)
		KEY_NEW_FAIL("out of memory");
	memcpy(kctx->raw_key_data, key_data, key_size);
	kctx->raw_key_size = key_size;

	if (strcmp(algo_name, "AES-CBC") == 0 ||
	    strcmp(algo_name, "AES-CTR") == 0) {
		bool ctr = strcmp(algo_name, "AES-CTR") == 0;
		if (key_size != 16 && key_size != 24 && key_size != 32)
			KEY_NEW_FAIL("Invalid key length");
		kctx->type = NX_CRYPTO_KEY_TYPE_SECRET;
		kctx->algorithm =
		    ctr ? NX_CRYPTO_KEY_ALGORITHM_AES_CTR
		        : NX_CRYPTO_KEY_ALGORITHM_AES_CBC;
		nx_crypto_key_aes_t *aes =
		    (nx_crypto_key_aes_t *)calloc(1, sizeof(nx_crypto_key_aes_t));
		if (!aes)
			KEY_NEW_FAIL("out of memory");
		kctx->handle = aes;
		aes->key_length = key_size;
		if (ctr) {
			if (kctx->usages & (NX_CRYPTO_KEY_USAGE_ENCRYPT |
			                    NX_CRYPTO_KEY_USAGE_DECRYPT)) {
				if (key_size == 16)
					aes128CtrContextCreate(&aes->decrypt.ctr_128, key_data,
					                       key_data);
				else if (key_size == 24)
					aes192CtrContextCreate(&aes->decrypt.ctr_192, key_data,
					                       key_data);
				else
					aes256CtrContextCreate(&aes->decrypt.ctr_256, key_data,
					                       key_data);
			}
		} else {
			if (key_size == 16) {
				if (kctx->usages & NX_CRYPTO_KEY_USAGE_ENCRYPT)
					aes128CbcContextCreate(&aes->encrypt.cbc_128, key_data,
					                       key_data, true);
				if (kctx->usages & NX_CRYPTO_KEY_USAGE_DECRYPT)
					aes128CbcContextCreate(&aes->decrypt.cbc_128, key_data,
					                       key_data, false);
			} else if (key_size == 24) {
				if (kctx->usages & NX_CRYPTO_KEY_USAGE_ENCRYPT)
					aes192CbcContextCreate(&aes->encrypt.cbc_192, key_data,
					                       key_data, true);
				if (kctx->usages & NX_CRYPTO_KEY_USAGE_DECRYPT)
					aes192CbcContextCreate(&aes->decrypt.cbc_192, key_data,
					                       key_data, false);
			} else {
				if (kctx->usages & NX_CRYPTO_KEY_USAGE_ENCRYPT)
					aes256CbcContextCreate(&aes->encrypt.cbc_256, key_data,
					                       key_data, true);
				if (kctx->usages & NX_CRYPTO_KEY_USAGE_DECRYPT)
					aes256CbcContextCreate(&aes->decrypt.cbc_256, key_data,
					                       key_data, false);
			}
		}
	} else if (strcmp(algo_name, "AES-XTS") == 0) {
		if (key_size != 32 && key_size != 48 && key_size != 64)
			KEY_NEW_FAIL("Invalid key length");
		kctx->type = NX_CRYPTO_KEY_TYPE_SECRET;
		kctx->algorithm = NX_CRYPTO_KEY_ALGORITHM_AES_XTS;
		nx_crypto_key_aes_t *aes =
		    (nx_crypto_key_aes_t *)calloc(1, sizeof(nx_crypto_key_aes_t));
		if (!aes)
			KEY_NEW_FAIL("out of memory");
		kctx->handle = aes;
		aes->key_length = key_size;
		if (key_size == 32) {
			if (kctx->usages & NX_CRYPTO_KEY_USAGE_ENCRYPT)
				aes128XtsContextCreate(&aes->encrypt.xts_128, key_data,
				                       key_data + 0x10, true);
			if (kctx->usages & NX_CRYPTO_KEY_USAGE_DECRYPT)
				aes128XtsContextCreate(&aes->decrypt.xts_128, key_data,
				                       key_data + 0x10, false);
		} else if (key_size == 48) {
			if (kctx->usages & NX_CRYPTO_KEY_USAGE_ENCRYPT)
				aes192XtsContextCreate(&aes->encrypt.xts_192, key_data,
				                       key_data + 0x18, true);
			if (kctx->usages & NX_CRYPTO_KEY_USAGE_DECRYPT)
				aes192XtsContextCreate(&aes->decrypt.xts_192, key_data,
				                       key_data + 0x18, false);
		} else {
			if (kctx->usages & NX_CRYPTO_KEY_USAGE_ENCRYPT)
				aes256XtsContextCreate(&aes->encrypt.xts_256, key_data,
				                       key_data + 0x20, true);
			if (kctx->usages & NX_CRYPTO_KEY_USAGE_DECRYPT)
				aes256XtsContextCreate(&aes->decrypt.xts_256, key_data,
				                       key_data + 0x20, false);
		}
	} else if (strcmp(algo_name, "PBKDF2") == 0) {
		kctx->type = NX_CRYPTO_KEY_TYPE_SECRET;
		kctx->algorithm = NX_CRYPTO_KEY_ALGORITHM_PBKDF2;
		kctx->handle = NULL;
	} else if (strcmp(algo_name, "HKDF") == 0) {
		kctx->type = NX_CRYPTO_KEY_TYPE_SECRET;
		kctx->algorithm = NX_CRYPTO_KEY_ALGORITHM_HKDF;
		kctx->handle = NULL;
	} else if (strcmp(algo_name, "AES-GCM") == 0) {
		if (key_size != 16 && key_size != 24 && key_size != 32)
			KEY_NEW_FAIL("Invalid key length");
		kctx->type = NX_CRYPTO_KEY_TYPE_SECRET;
		kctx->algorithm = NX_CRYPTO_KEY_ALGORITHM_AES_GCM;
	} else if (strcmp(algo_name, "HMAC") == 0) {
		kctx->type = NX_CRYPTO_KEY_TYPE_SECRET;
		kctx->algorithm = NX_CRYPTO_KEY_ALGORITHM_HMAC;
		char hash_name[16] = {0};
		if (!nx_crypto_read_hash_name(iso, algo, hash_name, sizeof(hash_name)))
			KEY_NEW_FAIL("expected hash");
		nx_crypto_key_hmac_t *hmac =
		    (nx_crypto_key_hmac_t *)calloc(1, sizeof(nx_crypto_key_hmac_t));
		if (!hmac)
			KEY_NEW_FAIL("out of memory");
		hmac->key = (uint8_t *)malloc(key_size);
		if (!hmac->key) {
			free(hmac);
			KEY_NEW_FAIL("out of memory");
		}
		memcpy(hmac->key, key_data, key_size);
		hmac->key_length = key_size;
		strncpy(hmac->hash_name, hash_name, sizeof(hmac->hash_name) - 1);
		kctx->handle = hmac;
	} else if (strcmp(algo_name, "ECDSA") == 0 ||
	           strcmp(algo_name, "ECDH") == 0) {
		kctx->type = NX_CRYPTO_KEY_TYPE_PUBLIC;
		kctx->algorithm = (strcmp(algo_name, "ECDSA") == 0)
		                      ? NX_CRYPTO_KEY_ALGORITHM_ECDSA
		                      : NX_CRYPTO_KEY_ALGORITHM_ECDH;
		Local<Value> curve_val;
		if (!algo->Get(context, nx_str(iso, "namedCurve")).ToLocal(&curve_val))
			KEY_NEW_FAIL("expected namedCurve");
		String::Utf8Value curve(iso, curve_val);
		const char *curve_name = *curve;
		mbedtls_ecp_group_id grp_id;
		if (curve_name && strcmp(curve_name, "P-256") == 0)
			grp_id = MBEDTLS_ECP_DP_SECP256R1;
		else if (curve_name && strcmp(curve_name, "P-384") == 0)
			grp_id = MBEDTLS_ECP_DP_SECP384R1;
		else
			KEY_NEW_FAIL("Unsupported curve");
		nx_crypto_key_ec_t *ec =
		    (nx_crypto_key_ec_t *)calloc(1, sizeof(nx_crypto_key_ec_t));
		if (!ec)
			KEY_NEW_FAIL("out of memory");
		mbedtls_ecp_keypair_init(&ec->keypair);
		strncpy(ec->curve_name, curve_name, sizeof(ec->curve_name) - 1);
		if (mbedtls_ecp_group_load(&ec->keypair.grp, grp_id) != 0) {
			mbedtls_ecp_keypair_free(&ec->keypair);
			free(ec);
			KEY_NEW_FAIL("Failed to load EC group");
		}
		if (mbedtls_ecp_point_read_binary(&ec->keypair.grp, &ec->keypair.Q,
		                                  key_data, key_size) != 0) {
			mbedtls_ecp_keypair_free(&ec->keypair);
			free(ec);
			KEY_NEW_FAIL("Failed to read EC public key");
		}
		kctx->handle = ec;
	} else {
		KEY_NEW_FAIL("Unrecognized algorithm name");
	}
#undef KEY_NEW_FAIL

	info.GetReturnValue().Set(nx_make_crypto_key(iso, kctx));
}

// ==================================================================
// sign() / verify()
// ==================================================================

struct nx_crypto_sign_async_t {
	int err = 0;
	Global<Value> algorithm_val;
	void *algorithm_params = nullptr;
	Global<Value> key_val;
	nx_crypto_key_t *key = nullptr;
	Global<Value> data_val;
	uint8_t *data = nullptr;
	size_t data_size = 0;
	void *result = nullptr;
	size_t result_size = 0;
};

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
		                          data->data, data->data_size,
		                          (uint8_t *)data->result);
		if (ret != 0) {
			free(data->result);
			data->result = nullptr;
			data->err = ret;
		}
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDSA) {
		nx_crypto_key_ec_t *ec = (nx_crypto_key_ec_t *)data->key->handle;
		const char *hash_name = (const char *)data->algorithm_params;
		mbedtls_md_type_t md_type = nx_crypto_get_md_type(hash_name);
		if (md_type == MBEDTLS_MD_NONE) {
			data->err = ENOTSUP;
			return;
		}
		const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(md_type);
		size_t hash_len = mbedtls_md_get_size(md_info);
		uint8_t hash[64];
		int ret = mbedtls_md(md_info, data->data, data->data_size, hash);
		if (ret != 0) {
			data->err = ret;
			return;
		}

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
		ret = mbedtls_ecdsa_write_signature(&ec->keypair, md_type, hash,
		                                    hash_len, der_sig, sizeof(der_sig),
		                                    &der_sig_len, mbedtls_ctr_drbg_random,
		                                    &ctr_drbg);
#else
		ret = mbedtls_ecdsa_write_signature(&ec->keypair, md_type, hash,
		                                    hash_len, der_sig, &der_sig_len,
		                                    mbedtls_ctr_drbg_random, &ctr_drbg);
#endif
		mbedtls_ctr_drbg_free(&ctr_drbg);
		mbedtls_entropy_free(&entropy);
		if (ret != 0) {
			data->err = ret;
			return;
		}

		size_t coord_size = mbedtls_mpi_size(&ec->keypair.grp.P);
		data->result_size = coord_size * 2;
		data->result = calloc(1, data->result_size);
		if (!data->result) {
			data->err = ENOMEM;
			return;
		}

		mbedtls_mpi r, s;
		mbedtls_mpi_init(&r);
		mbedtls_mpi_init(&s);
		{
			uint8_t *p = der_sig;
			size_t len;
			if (*p != 0x30) {
				data->err = EINVAL;
				goto sign_cleanup;
			}
			p++;
			if (*p & 0x80) {
				p += (*p & 0x7f) + 1;
			} else {
				p++;
			}
			if (*p != 0x02) {
				data->err = EINVAL;
				goto sign_cleanup;
			}
			p++;
			len = *p++;
			ret = mbedtls_mpi_read_binary(&r, p, len);
			if (ret != 0) {
				data->err = ret;
				goto sign_cleanup;
			}
			p += len;
			if (*p != 0x02) {
				data->err = EINVAL;
				goto sign_cleanup;
			}
			p++;
			len = *p++;
			ret = mbedtls_mpi_read_binary(&s, p, len);
			if (ret != 0) {
				data->err = ret;
				goto sign_cleanup;
			}
			ret = mbedtls_mpi_write_binary(&r, (uint8_t *)data->result,
			                               coord_size);
			if (ret != 0) {
				data->err = ret;
				goto sign_cleanup;
			}
			ret = mbedtls_mpi_write_binary(
			    &s, (uint8_t *)data->result + coord_size, coord_size);
			if (ret != 0) {
				data->err = ret;
				goto sign_cleanup;
			}
		}
	sign_cleanup:
		mbedtls_mpi_free(&r);
		mbedtls_mpi_free(&s);
		if (data->err) {
			free(data->result);
			data->result = nullptr;
		}
	} else if (data->key->algorithm ==
	           NX_CRYPTO_KEY_ALGORITHM_RSASSA_PKCS1_V1_5) {
		nx_crypto_key_rsa_t *rsa = (nx_crypto_key_rsa_t *)data->key->handle;
		mbedtls_md_type_t md_type = nx_crypto_get_md_type(rsa->hash_name);
		if (md_type == MBEDTLS_MD_NONE) {
			data->err = ENOTSUP;
			return;
		}
		const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(md_type);
		size_t hash_len = mbedtls_md_get_size(md_info);
		uint8_t hash_buf[64];
		int ret = mbedtls_md(md_info, data->data, data->data_size, hash_buf);
		if (ret != 0) {
			data->err = ret;
			return;
		}
		size_t rsa_len = mbedtls_rsa_get_len(&rsa->rsa);
		data->result = malloc(rsa_len);
		if (!data->result) {
			data->err = ENOMEM;
			return;
		}
		data->result_size = rsa_len;
		mbedtls_rsa_set_padding(&rsa->rsa, MBEDTLS_RSA_PKCS_V15,
		                        MBEDTLS_MD_NONE);
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
			data->result = nullptr;
			data->err = ret;
			return;
		}
#if MBEDTLS_VERSION_MAJOR >= 3
		ret = mbedtls_rsa_rsassa_pkcs1_v15_sign(
		    &rsa->rsa, mbedtls_ctr_drbg_random, &sign_ctr_drbg, md_type,
		    hash_len, hash_buf, (uint8_t *)data->result);
#else
		ret = mbedtls_rsa_rsassa_pkcs1_v15_sign(
		    &rsa->rsa, mbedtls_ctr_drbg_random, &sign_ctr_drbg,
		    MBEDTLS_RSA_PRIVATE, md_type, hash_len, hash_buf,
		    (uint8_t *)data->result);
#endif
		mbedtls_ctr_drbg_free(&sign_ctr_drbg);
		mbedtls_entropy_free(&sign_entropy);
		if (ret != 0) {
			free(data->result);
			data->result = nullptr;
			data->err = ret;
		}
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_PSS) {
		nx_crypto_key_rsa_t *rsa = (nx_crypto_key_rsa_t *)data->key->handle;
		mbedtls_md_type_t md_type = nx_crypto_get_md_type(rsa->hash_name);
		if (md_type == MBEDTLS_MD_NONE) {
			data->err = ENOTSUP;
			return;
		}
		const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(md_type);
		size_t hash_len = mbedtls_md_get_size(md_info);
		uint8_t hash_buf2[64];
		int ret = mbedtls_md(md_info, data->data, data->data_size, hash_buf2);
		if (ret != 0) {
			data->err = ret;
			return;
		}
		size_t rsa_len = mbedtls_rsa_get_len(&rsa->rsa);
		data->result = malloc(rsa_len);
		if (!data->result) {
			data->err = ENOMEM;
			return;
		}
		data->result_size = rsa_len;
		int salt_len = rsa->salt_length;
		if (data->algorithm_params)
			salt_len = *(int *)data->algorithm_params;
		if (salt_len < 0)
			salt_len = (int)hash_len;
		mbedtls_entropy_context pss_entropy;
		mbedtls_ctr_drbg_context pss_ctr_drbg;
		mbedtls_entropy_init(&pss_entropy);
		mbedtls_ctr_drbg_init(&pss_ctr_drbg);
		ret = mbedtls_ctr_drbg_seed(&pss_ctr_drbg, mbedtls_entropy_func,
		                            &pss_entropy, NULL, 0);
		if (ret != 0) {
			mbedtls_ctr_drbg_free(&pss_ctr_drbg);
			mbedtls_entropy_free(&pss_entropy);
			free(data->result);
			data->result = nullptr;
			data->err = ret;
			return;
		}
		mbedtls_rsa_set_padding(&rsa->rsa, MBEDTLS_RSA_PKCS_V21, md_type);
#if MBEDTLS_VERSION_MAJOR >= 3
		ret = mbedtls_rsa_rsassa_pss_sign_ext(
		    &rsa->rsa, mbedtls_ctr_drbg_random, &pss_ctr_drbg, md_type, hash_len,
		    hash_buf2, salt_len, (uint8_t *)data->result);
#else
		if (salt_len != (int)hash_len) {
			mbedtls_ctr_drbg_free(&pss_ctr_drbg);
			mbedtls_entropy_free(&pss_entropy);
			free(data->result);
			data->result = nullptr;
			data->err = ENOTSUP;
			return;
		}
		ret = mbedtls_rsa_rsassa_pss_sign(
		    &rsa->rsa, mbedtls_ctr_drbg_random, &pss_ctr_drbg,
		    MBEDTLS_RSA_PRIVATE, md_type, hash_len, hash_buf2,
		    (uint8_t *)data->result);
#endif
		mbedtls_ctr_drbg_free(&pss_ctr_drbg);
		mbedtls_entropy_free(&pss_entropy);
		if (ret != 0) {
			free(data->result);
			data->result = nullptr;
			data->err = ENOTSUP;
		}
	} else {
		data->err = ENOTSUP;
	}
}

MaybeLocal<Value> nx_crypto_sign_cb(Isolate *iso, nx_work_t *req) {
	nx_crypto_sign_async_t *data = (nx_crypto_sign_async_t *)req->data;
	data->algorithm_val.Reset();
	data->key_val.Reset();
	data->data_val.Reset();
	if (data->algorithm_params) {
		free(data->algorithm_params);
		data->algorithm_params = nullptr;
	}
	if (data->err) {
		nx_throw(iso, strerror(data->err));
		return MaybeLocal<Value>();
	}
	void *result = data->result;
	size_t result_size = data->result_size;
	data->result = nullptr;
	return nx_ab_take(iso, result, result_size);
}

// Extract hash/salt params for sign+verify (ECDSA hash name, RSA-PSS salt).
void nx_crypto_extract_sign_params(Isolate *iso, Local<Value> algorithm,
                                   nx_crypto_key_t *key, void **out_params) {
	Local<Context> context = iso->GetCurrentContext();
	Local<Object> algo =
	    algorithm->IsObject() ? algorithm.As<Object>() : Local<Object>();
	*out_params = nullptr;
	if (key->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDSA && !algo.IsEmpty()) {
		char hash_name[16] = {0};
		if (nx_crypto_read_hash_name(iso, algo, hash_name, sizeof(hash_name))) {
			*out_params = strdup(hash_name);
		}
	} else if (key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_PSS &&
	           !algo.IsEmpty()) {
		Local<Value> salt_val;
		if (algo->Get(context, nx_str(iso, "saltLength")).ToLocal(&salt_val) &&
		    !salt_val->IsUndefined()) {
			int *salt_len_ptr = (int *)malloc(sizeof(int));
			uint32_t sl = 0;
			if (salt_val->Uint32Value(context).To(&sl))
				*salt_len_ptr = (int)sl;
			else
				*salt_len_ptr = -1;
			*out_params = salt_len_ptr;
		}
	}
}

void nx_crypto_sign(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	NX_INIT_WORK_T_CPP(nx_crypto_sign_async_t);

	data->key = nx_get_crypto_key(info[1]);
	if (!data->key) {
		req->data_dtor(req->data);
		delete req;
		nx_throw(iso, "invalid key");
		return;
	}
	if (!(data->key->usages & NX_CRYPTO_KEY_USAGE_SIGN)) {
		req->data_dtor(req->data);
		delete req;
		nx_throw(iso, "Key does not support the 'sign' operation");
		return;
	}
	data->data = NX_GetBufferSource(iso, &data->data_size, info[2]);
	if (!data->data) {
		req->data_dtor(req->data);
		delete req;
		nx_throw(iso, "expected ArrayBuffer");
		return;
	}
	nx_crypto_extract_sign_params(iso, info[0], data->key,
	                              &data->algorithm_params);
	data->algorithm_val.Reset(iso, info[0]);
	data->key_val.Reset(iso, info[1]);
	data->data_val.Reset(iso, info[2]);
	info.GetReturnValue().Set(
	    nx_queue_async(iso, req, nx_crypto_sign_do, nx_crypto_sign_cb));
}

struct nx_crypto_verify_async_t {
	int err = 0;
	Global<Value> algorithm_val;
	void *algorithm_params = nullptr;
	Global<Value> key_val;
	nx_crypto_key_t *key = nullptr;
	Global<Value> signature_val;
	uint8_t *signature = nullptr;
	size_t signature_size = 0;
	Global<Value> data_val;
	uint8_t *data = nullptr;
	size_t data_size = 0;
	bool result = false;
};

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
		uint8_t *computed = (uint8_t *)calloc(1, mac_size);
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
		const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(md_type);
		size_t hash_len = mbedtls_md_get_size(md_info);
		uint8_t hash[64];
		int ret = mbedtls_md(md_info, data->data, data->data_size, hash);
		if (ret != 0) {
			data->err = ret;
			return;
		}
		size_t coord_size = mbedtls_mpi_size(&ec->keypair.grp.P);
		if (data->signature_size != coord_size * 2) {
			data->result = false;
			return;
		}
		mbedtls_mpi r, s;
		mbedtls_mpi_init(&r);
		mbedtls_mpi_init(&s);
		mbedtls_mpi_read_binary(&r, data->signature, coord_size);
		mbedtls_mpi_read_binary(&s, data->signature + coord_size, coord_size);
		ret = mbedtls_ecdsa_verify(&ec->keypair.grp, hash, hash_len,
		                           &ec->keypair.Q, &r, &s);
		data->result = (ret == 0);
		mbedtls_mpi_free(&r);
		mbedtls_mpi_free(&s);
	} else if (data->key->algorithm ==
	           NX_CRYPTO_KEY_ALGORITHM_RSASSA_PKCS1_V1_5) {
		nx_crypto_key_rsa_t *rsa = (nx_crypto_key_rsa_t *)data->key->handle;
		mbedtls_md_type_t md_type = nx_crypto_get_md_type(rsa->hash_name);
		if (md_type == MBEDTLS_MD_NONE) {
			data->err = ENOTSUP;
			return;
		}
		const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(md_type);
		size_t hash_len = mbedtls_md_get_size(md_info);
		uint8_t hash_buf[64];
		int ret = mbedtls_md(md_info, data->data, data->data_size, hash_buf);
		if (ret != 0) {
			data->err = ret;
			return;
		}
		mbedtls_rsa_set_padding(&rsa->rsa, MBEDTLS_RSA_PKCS_V15,
		                        MBEDTLS_MD_NONE);
#if MBEDTLS_VERSION_MAJOR >= 3
		ret = mbedtls_rsa_rsassa_pkcs1_v15_verify(&rsa->rsa, md_type, hash_len,
		                                          hash_buf, data->signature);
#else
		ret = mbedtls_rsa_rsassa_pkcs1_v15_verify(
		    &rsa->rsa, NULL, NULL, MBEDTLS_RSA_PUBLIC, md_type, hash_len,
		    hash_buf, data->signature);
#endif
		data->result = (ret == 0);
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_PSS) {
		nx_crypto_key_rsa_t *rsa = (nx_crypto_key_rsa_t *)data->key->handle;
		mbedtls_md_type_t md_type = nx_crypto_get_md_type(rsa->hash_name);
		if (md_type == MBEDTLS_MD_NONE) {
			data->err = ENOTSUP;
			return;
		}
		const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(md_type);
		size_t hash_len = mbedtls_md_get_size(md_info);
		uint8_t hash_buf2[64];
		int ret = mbedtls_md(md_info, data->data, data->data_size, hash_buf2);
		if (ret != 0) {
			data->err = ret;
			return;
		}
		int salt_len = rsa->salt_length;
		if (data->algorithm_params)
			salt_len = *(int *)data->algorithm_params;
		if (salt_len < 0)
			salt_len = (int)hash_len;
		mbedtls_rsa_set_padding(&rsa->rsa, MBEDTLS_RSA_PKCS_V21, md_type);
#if MBEDTLS_VERSION_MAJOR >= 3
		ret = mbedtls_rsa_rsassa_pss_verify_ext(&rsa->rsa, md_type, hash_len,
		                                        hash_buf2, md_type, salt_len,
		                                        data->signature);
#else
		ret = mbedtls_rsa_rsassa_pss_verify_ext(
		    &rsa->rsa, NULL, NULL, MBEDTLS_RSA_PUBLIC, md_type, hash_len,
		    hash_buf2, md_type, salt_len, data->signature);
#endif
		data->result = (ret == 0);
	} else {
		data->err = ENOTSUP;
	}
}

MaybeLocal<Value> nx_crypto_verify_cb(Isolate *iso, nx_work_t *req) {
	nx_crypto_verify_async_t *data = (nx_crypto_verify_async_t *)req->data;
	data->algorithm_val.Reset();
	data->key_val.Reset();
	data->signature_val.Reset();
	data->data_val.Reset();
	if (data->algorithm_params) {
		free(data->algorithm_params);
		data->algorithm_params = nullptr;
	}
	if (data->err) {
		nx_throw(iso, strerror(data->err));
		return MaybeLocal<Value>();
	}
	return Boolean::New(iso, data->result).As<Value>();
}

void nx_crypto_verify(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	NX_INIT_WORK_T_CPP(nx_crypto_verify_async_t);

	data->key = nx_get_crypto_key(info[1]);
	if (!data->key) {
		req->data_dtor(req->data);
		delete req;
		nx_throw(iso, "invalid key");
		return;
	}
	if (!(data->key->usages & NX_CRYPTO_KEY_USAGE_VERIFY)) {
		req->data_dtor(req->data);
		delete req;
		nx_throw(iso, "Key does not support the 'verify' operation");
		return;
	}
	data->signature = NX_GetBufferSource(iso, &data->signature_size, info[2]);
	if (!data->signature) {
		req->data_dtor(req->data);
		delete req;
		nx_throw(iso, "expected signature ArrayBuffer");
		return;
	}
	data->data = NX_GetBufferSource(iso, &data->data_size, info[3]);
	if (!data->data) {
		req->data_dtor(req->data);
		delete req;
		nx_throw(iso, "expected data ArrayBuffer");
		return;
	}
	nx_crypto_extract_sign_params(iso, info[0], data->key,
	                              &data->algorithm_params);
	data->algorithm_val.Reset(iso, info[0]);
	data->key_val.Reset(iso, info[1]);
	data->signature_val.Reset(iso, info[2]);
	data->data_val.Reset(iso, info[3]);
	info.GetReturnValue().Set(
	    nx_queue_async(iso, req, nx_crypto_verify_do, nx_crypto_verify_cb));
}

// ==================================================================
// exportKey("raw"), generateKey (EC), importKey (EC private), deriveBits
// ==================================================================

void nx_crypto_export_key(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value format(iso, info[0]);
	if (!*format) {
		nx_throw(iso, "expected format");
		return;
	}
	nx_crypto_key_t *key = nx_get_crypto_key(info[1]);
	if (!key) {
		nx_throw(iso, "invalid key");
		return;
	}
	if (strcmp(*format, "raw") != 0) {
		nx_throw(iso, "Only 'raw' export format is supported");
		return;
	}
	if (!key->extractable) {
		nx_throw(iso, "Key is not extractable");
		return;
	}
	if (!key->raw_key_data || key->raw_key_size == 0) {
		nx_throw(iso, "Key does not have raw material");
		return;
	}
	uint8_t *copy = (uint8_t *)malloc(key->raw_key_size);
	if (!copy) {
		nx_throw(iso, "out of memory");
		return;
	}
	memcpy(copy, key->raw_key_data, key->raw_key_size);
	info.GetReturnValue().Set(nx_ab_take(iso, copy, key->raw_key_size));
}

void nx_crypto_generate_key_ec(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	String::Utf8Value curve(iso, info[0]);
	const char *curve_name = *curve;
	mbedtls_ecp_group_id grp_id;
	if (curve_name && strcmp(curve_name, "P-256") == 0)
		grp_id = MBEDTLS_ECP_DP_SECP256R1;
	else if (curve_name && strcmp(curve_name, "P-384") == 0)
		grp_id = MBEDTLS_ECP_DP_SECP384R1;
	else {
		nx_throw(iso, "Unsupported curve");
		return;
	}

	mbedtls_ecp_keypair kp;
	mbedtls_ecp_keypair_init(&kp);
	int ret = mbedtls_ecp_group_load(&kp.grp, grp_id);
	if (ret != 0) {
		mbedtls_ecp_keypair_free(&kp);
		nx_throw(iso, "Failed to load EC group");
		return;
	}
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_entropy_init(&entropy);
	mbedtls_ctr_drbg_init(&ctr_drbg);
	ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL,
	                            0);
	if (ret != 0) {
		mbedtls_ctr_drbg_free(&ctr_drbg);
		mbedtls_entropy_free(&entropy);
		mbedtls_ecp_keypair_free(&kp);
		nx_throw(iso, "Failed to seed DRBG");
		return;
	}
	ret = mbedtls_ecp_gen_keypair(&kp.grp, &kp.d, &kp.Q,
	                              mbedtls_ctr_drbg_random, &ctr_drbg);
	mbedtls_ctr_drbg_free(&ctr_drbg);
	mbedtls_entropy_free(&entropy);
	if (ret != 0) {
		mbedtls_ecp_keypair_free(&kp);
		nx_throw(iso, "Failed to generate EC keypair");
		return;
	}

	size_t pub_len = 0;
	size_t coord_size = mbedtls_mpi_size(&kp.grp.P);
	size_t pub_buf_size = 1 + 2 * coord_size;
	uint8_t *pub_buf = (uint8_t *)malloc(pub_buf_size);
	if (!pub_buf) {
		mbedtls_ecp_keypair_free(&kp);
		nx_throw(iso, "out of memory");
		return;
	}
	ret = mbedtls_ecp_point_write_binary(&kp.grp, &kp.Q,
	                                     MBEDTLS_ECP_PF_UNCOMPRESSED, &pub_len,
	                                     pub_buf, pub_buf_size);
	if (ret != 0) {
		free(pub_buf);
		mbedtls_ecp_keypair_free(&kp);
		nx_throw(iso, "Failed to export public key");
		return;
	}
	size_t priv_len = mbedtls_mpi_size(&kp.d);
	uint8_t *priv_buf = (uint8_t *)malloc(priv_len);
	if (!priv_buf) {
		free(pub_buf);
		mbedtls_ecp_keypair_free(&kp);
		nx_throw(iso, "out of memory");
		return;
	}
	ret = mbedtls_mpi_write_binary(&kp.d, priv_buf, priv_len);
	mbedtls_ecp_keypair_free(&kp);
	if (ret != 0) {
		free(pub_buf);
		free(priv_buf);
		nx_throw(iso, "Failed to export private key");
		return;
	}

	Local<Array> result = Array::New(iso);
	result->Set(context, 0, nx_ab_take(iso, pub_buf, pub_len)).Check();
	result->Set(context, 1, nx_ab_take(iso, priv_buf, priv_len)).Check();
	info.GetReturnValue().Set(result);
}

// argv[0]=algorithm obj, argv[1]=private scalar, argv[2]=public point,
// argv[3]=extractable, argv[4]=usages
void nx_crypto_key_new_ec_private(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();

	nx_crypto_key_t *kctx =
	    (nx_crypto_key_t *)calloc(1, sizeof(nx_crypto_key_t));
	if (!kctx) {
		nx_throw(iso, "out of memory");
		return;
	}
	new (&kctx->algorithm_cached) Global<Value>();
	new (&kctx->usages_cached) Global<Value>();

	kctx->extractable = info[3]->BooleanValue(iso);

	nx_crypto_key_usage usages;
	nx_crypto_parse_usages(iso, info[4], &usages, false);
	kctx->usages = usages;
	kctx->type = NX_CRYPTO_KEY_TYPE_PRIVATE;

	Local<Object> algo = info[0].As<Object>();
	Local<Value> name_val;
	algo->Get(context, nx_str(iso, "name")).ToLocal(&name_val);
	String::Utf8Value algo_name(iso, name_val);
	kctx->algorithm = (*algo_name && strcmp(*algo_name, "ECDSA") == 0)
	                      ? NX_CRYPTO_KEY_ALGORITHM_ECDSA
	                      : NX_CRYPTO_KEY_ALGORITHM_ECDH;

	Local<Value> curve_val;
	algo->Get(context, nx_str(iso, "namedCurve")).ToLocal(&curve_val);
	String::Utf8Value curve(iso, curve_val);
	const char *curve_name = *curve;
	mbedtls_ecp_group_id grp_id;
	if (curve_name && strcmp(curve_name, "P-256") == 0)
		grp_id = MBEDTLS_ECP_DP_SECP256R1;
	else if (curve_name && strcmp(curve_name, "P-384") == 0)
		grp_id = MBEDTLS_ECP_DP_SECP384R1;
	else {
		free(kctx);
		nx_throw(iso, "Unsupported curve");
		return;
	}

	nx_crypto_key_ec_t *ec =
	    (nx_crypto_key_ec_t *)calloc(1, sizeof(nx_crypto_key_ec_t));
	if (!ec) {
		free(kctx);
		nx_throw(iso, "out of memory");
		return;
	}
	mbedtls_ecp_keypair_init(&ec->keypair);
	strncpy(ec->curve_name, curve_name, sizeof(ec->curve_name) - 1);

	if (mbedtls_ecp_group_load(&ec->keypair.grp, grp_id) != 0) {
		mbedtls_ecp_keypair_free(&ec->keypair);
		free(ec);
		free(kctx);
		nx_throw(iso, "Failed to load EC group");
		return;
	}

	size_t priv_size = 0;
	const uint8_t *priv_data = NX_GetBufferSource(iso, &priv_size, info[1]);
	if (!priv_data ||
	    mbedtls_mpi_read_binary(&ec->keypair.d, priv_data, priv_size) != 0) {
		mbedtls_ecp_keypair_free(&ec->keypair);
		free(ec);
		free(kctx);
		nx_throw(iso, "Failed to read private key");
		return;
	}
	size_t pub_size = 0;
	const uint8_t *pub_data = NX_GetBufferSource(iso, &pub_size, info[2]);
	if (!pub_data ||
	    mbedtls_ecp_point_read_binary(&ec->keypair.grp, &ec->keypair.Q,
	                                  pub_data, pub_size) != 0) {
		mbedtls_ecp_keypair_free(&ec->keypair);
		free(ec);
		free(kctx);
		nx_throw(iso, "Failed to read public key");
		return;
	}

	kctx->handle = ec;
	kctx->raw_key_data = (uint8_t *)malloc(priv_size);
	if (kctx->raw_key_data) {
		memcpy(kctx->raw_key_data, priv_data, priv_size);
		kctx->raw_key_size = priv_size;
	}
	info.GetReturnValue().Set(nx_make_crypto_key(iso, kctx));
}

struct nx_crypto_derive_bits_async_t {
	int err = 0;
	Global<Value> algorithm_val;
	Global<Value> key_val;
	nx_crypto_key_t *key = nullptr;
	Global<Value> public_key_val;
	nx_crypto_key_t *public_key = nullptr;
	char hash_name[16] = {0};
	uint8_t *salt = nullptr;
	size_t salt_size = 0;
	uint32_t iterations = 0;
	uint8_t *info_buf = nullptr;
	size_t info_size = 0;
	size_t length = 0;
	void *result = nullptr;
	size_t result_size = 0;
};

void nx_crypto_derive_bits_do(nx_work_t *req) {
	nx_crypto_derive_bits_async_t *data =
	    (nx_crypto_derive_bits_async_t *)req->data;

	if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDH) {
		nx_crypto_key_ec_t *priv_ec = (nx_crypto_key_ec_t *)data->key->handle;
		nx_crypto_key_ec_t *pub_ec =
		    (nx_crypto_key_ec_t *)data->public_key->handle;

		mbedtls_mpi shared;
		mbedtls_mpi_init(&shared);
		mbedtls_entropy_context entropy;
		mbedtls_ctr_drbg_context ctr_drbg;
		mbedtls_entropy_init(&entropy);
		mbedtls_ctr_drbg_init(&ctr_drbg);
		int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func,
		                                &entropy, NULL, 0);
		if (ret != 0) {
			mbedtls_ctr_drbg_free(&ctr_drbg);
			mbedtls_entropy_free(&entropy);
			mbedtls_mpi_free(&shared);
			data->err = ret;
			return;
		}
		ret = mbedtls_ecdh_compute_shared(&priv_ec->keypair.grp, &shared,
		                                  &pub_ec->keypair.Q,
		                                  &priv_ec->keypair.d,
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
		if (out_bytes == 0)
			out_bytes = coord_size;
		data->result = calloc(1, out_bytes);
		if (!data->result) {
			mbedtls_mpi_free(&shared);
			data->err = ENOMEM;
			return;
		}
		uint8_t *full = (uint8_t *)calloc(1, coord_size);
		if (!full) {
			free(data->result);
			data->result = nullptr;
			mbedtls_mpi_free(&shared);
			data->err = ENOMEM;
			return;
		}
		mbedtls_mpi_write_binary(&shared, full, coord_size);
		memcpy(data->result, full,
		       out_bytes < coord_size ? out_bytes : coord_size);
		free(full);
		data->result_size = out_bytes;
		mbedtls_mpi_free(&shared);
		return;
	}

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
			data->result = nullptr;
			data->err = ret;
			return;
		}
		ret = mbedtls_pkcs5_pbkdf2_hmac(
		    &md_ctx, data->key->raw_key_data, data->key->raw_key_size,
		    data->salt, data->salt_size, data->iterations, data->length,
		    (uint8_t *)data->result);
		mbedtls_md_free(&md_ctx);
	} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_HKDF) {
		ret = mbedtls_hkdf(md_info, data->salt, data->salt_size,
		                   data->key->raw_key_data, data->key->raw_key_size,
		                   data->info_buf, data->info_size,
		                   (uint8_t *)data->result, data->length);
	} else {
		ret = ENOTSUP;
	}
	if (ret != 0) {
		free(data->result);
		data->result = nullptr;
		data->err = ret;
	}
}

MaybeLocal<Value> nx_crypto_derive_bits_cb(Isolate *iso, nx_work_t *req) {
	nx_crypto_derive_bits_async_t *data =
	    (nx_crypto_derive_bits_async_t *)req->data;
	data->algorithm_val.Reset();
	data->key_val.Reset();
	data->public_key_val.Reset();
	if (data->err) {
		nx_throw(iso, strerror(data->err));
		return MaybeLocal<Value>();
	}
	void *result = data->result;
	size_t result_size = data->result_size;
	data->result = nullptr;
	return nx_ab_take(iso, result, result_size);
}

void nx_crypto_derive_bits(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	NX_INIT_WORK_T_CPP(nx_crypto_derive_bits_async_t);

	data->key = nx_get_crypto_key(info[1]);
	if (!data->key) {
		req->data_dtor(req->data);
		delete req;
		nx_throw(iso, "invalid key");
		return;
	}
	if (!(data->key->usages & NX_CRYPTO_KEY_USAGE_DERIVE_BITS)) {
		req->data_dtor(req->data);
		delete req;
		nx_throw(iso, "Key does not support the 'deriveBits' operation");
		return;
	}
	uint32_t length_bits = 0;
	if (!info[2]->Uint32Value(context).To(&length_bits)) {
		req->data_dtor(req->data);
		delete req;
		return;
	}
	data->length = length_bits / 8;

	Local<Object> algo = info[0].As<Object>();
	if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_ECDH) {
		Local<Value> pub_val;
		if (!algo->Get(context, nx_str(iso, "public")).ToLocal(&pub_val)) {
			req->data_dtor(req->data);
			delete req;
			return;
		}
		data->public_key = nx_get_crypto_key(pub_val);
		if (!data->public_key) {
			req->data_dtor(req->data);
			delete req;
			nx_throw(iso, "Missing public key in algorithm");
			return;
		}
		data->public_key_val.Reset(iso, pub_val);
	} else {
		if (!nx_crypto_read_hash_name(iso, algo, data->hash_name,
		                              sizeof(data->hash_name))) {
			req->data_dtor(req->data);
			delete req;
			nx_throw(iso, "expected hash");
			return;
		}
		Local<Value> salt_val;
		if (algo->Get(context, nx_str(iso, "salt")).ToLocal(&salt_val)) {
			data->salt = NX_GetBufferSource(iso, &data->salt_size, salt_val);
		}
		if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_PBKDF2) {
			if (!data->salt) {
				req->data_dtor(req->data);
				delete req;
				nx_throw(iso, "expected salt");
				return;
			}
			Local<Value> iter_val;
			uint32_t iterations = 0;
			if (algo->Get(context, nx_str(iso, "iterations"))
			        .ToLocal(&iter_val)) {
				iter_val->Uint32Value(context).To(&iterations);
			}
			data->iterations = iterations;
		} else if (data->key->algorithm == NX_CRYPTO_KEY_ALGORITHM_HKDF) {
			if (!data->salt) {
				data->salt = (uint8_t *)"";
				data->salt_size = 0;
			}
			Local<Value> info_val;
			if (algo->Get(context, nx_str(iso, "info")).ToLocal(&info_val)) {
				data->info_buf =
				    NX_GetBufferSource(iso, &data->info_size, info_val);
			}
			if (!data->info_buf) {
				data->info_buf = (uint8_t *)"";
				data->info_size = 0;
			}
		}
	}

	data->algorithm_val.Reset(iso, info[0]);
	data->key_val.Reset(iso, info[1]);
	info.GetReturnValue().Set(nx_queue_async(
	    iso, req, nx_crypto_derive_bits_do, nx_crypto_derive_bits_cb));
}

// ==================================================================
// RSA generateKey (async)
// ==================================================================

struct nx_crypto_generate_key_rsa_async_t {
	int err = 0;
	uint32_t modulus_length = 0;
	uint32_t public_exponent = 0;
	uint8_t *n = nullptr; size_t n_len = 0;
	uint8_t *e = nullptr; size_t e_len = 0;
	uint8_t *d = nullptr; size_t d_len = 0;
	uint8_t *p = nullptr; size_t p_len = 0;
	uint8_t *q = nullptr; size_t q_len = 0;
	uint8_t *dp = nullptr; size_t dp_len = 0;
	uint8_t *dq = nullptr; size_t dq_len = 0;
	uint8_t *qi = nullptr; size_t qi_len = 0;
};

void nx_crypto_generate_key_rsa_do(nx_work_t *req) {
	nx_crypto_generate_key_rsa_async_t *data =
	    (nx_crypto_generate_key_rsa_async_t *)req->data;

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
	int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
	                                NULL, 0);
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
	mbedtls_mpi_init(&mpi_dp); mbedtls_mpi_init(&mpi_dq);
	mbedtls_mpi_init(&mpi_qi);

	mbedtls_rsa_export(&rsa, &mpi_n, &mpi_p, &mpi_q, &mpi_d, &mpi_e);

	mbedtls_mpi mpi_p1, mpi_q1;
	mbedtls_mpi_init(&mpi_p1); mbedtls_mpi_init(&mpi_q1);
	mbedtls_mpi_sub_int(&mpi_p1, &mpi_p, 1);
	mbedtls_mpi_sub_int(&mpi_q1, &mpi_q, 1);
	mbedtls_mpi_mod_mpi(&mpi_dp, &mpi_d, &mpi_p1);
	mbedtls_mpi_mod_mpi(&mpi_dq, &mpi_d, &mpi_q1);
	mbedtls_mpi_inv_mod(&mpi_qi, &mpi_q, &mpi_p);
	mbedtls_mpi_free(&mpi_p1); mbedtls_mpi_free(&mpi_q1);

#define EXPORT_MPI(field)                                                      \
	data->field##_len = mbedtls_mpi_size(&mpi_##field);                        \
	data->field = (uint8_t *)malloc(data->field##_len);                        \
	if (!data->field) {                                                        \
		data->err = ENOMEM;                                                    \
		goto rsa_gen_cleanup;                                                  \
	}                                                                          \
	mbedtls_mpi_write_binary(&mpi_##field, data->field, data->field##_len);

	EXPORT_MPI(n); EXPORT_MPI(e); EXPORT_MPI(d);
	EXPORT_MPI(p); EXPORT_MPI(q);
	EXPORT_MPI(dp); EXPORT_MPI(dq); EXPORT_MPI(qi);
#undef EXPORT_MPI

rsa_gen_cleanup:
	mbedtls_mpi_free(&mpi_n); mbedtls_mpi_free(&mpi_p); mbedtls_mpi_free(&mpi_q);
	mbedtls_mpi_free(&mpi_d); mbedtls_mpi_free(&mpi_e);
	mbedtls_mpi_free(&mpi_dp); mbedtls_mpi_free(&mpi_dq);
	mbedtls_mpi_free(&mpi_qi);
	mbedtls_rsa_free(&rsa);
	if (data->err) {
		free(data->n); free(data->e); free(data->d);
		free(data->p); free(data->q);
		free(data->dp); free(data->dq); free(data->qi);
		data->n = data->e = data->d = data->p = data->q = data->dp = data->dq =
		    data->qi = nullptr;
	}
}

MaybeLocal<Value> nx_crypto_generate_key_rsa_cb(Isolate *iso, nx_work_t *req) {
	nx_crypto_generate_key_rsa_async_t *data =
	    (nx_crypto_generate_key_rsa_async_t *)req->data;
	if (data->err) {
		char err_buf[128];
		mbedtls_strerror(data->err, err_buf, sizeof(err_buf));
		nx_throw(iso, err_buf);
		return MaybeLocal<Value>();
	}
	Local<Context> context = iso->GetCurrentContext();
	Local<Array> result = Array::New(iso);
#define SET_BUF(idx, field)                                                    \
	{                                                                          \
		uint8_t *b = data->field;                                              \
		size_t l = data->field##_len;                                          \
		data->field = nullptr;                                                 \
		result->Set(context, idx, nx_ab_take(iso, b, l)).Check();              \
	}
	SET_BUF(0, n); SET_BUF(1, e); SET_BUF(2, d);
	SET_BUF(3, p); SET_BUF(4, q);
	SET_BUF(5, dp); SET_BUF(6, dq); SET_BUF(7, qi);
#undef SET_BUF
	return result.As<Value>();
}

void nx_crypto_generate_key_rsa(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	NX_INIT_WORK_T_CPP(nx_crypto_generate_key_rsa_async_t);
	uint32_t modulus_length = 0, public_exponent = 0;
	if (!info[0]->Uint32Value(context).To(&modulus_length) ||
	    !info[1]->Uint32Value(context).To(&public_exponent)) {
		req->data_dtor(req->data);
		delete req;
		return;
	}
	data->modulus_length = modulus_length;
	data->public_exponent = public_exponent;
	info.GetReturnValue().Set(nx_queue_async(
	    iso, req, nx_crypto_generate_key_rsa_do, nx_crypto_generate_key_rsa_cb));
}

// ==================================================================
// RSA importKey from components (sync)
// argv: 0=algo name, 1=hash name, 2=type, 3=n, 4=e, 5=d, 6=p, 7=q,
//       8=extractable, 9=usages
// ==================================================================

void nx_crypto_key_new_rsa(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();

	nx_crypto_key_t *kctx =
	    (nx_crypto_key_t *)calloc(1, sizeof(nx_crypto_key_t));
	if (!kctx) {
		nx_throw(iso, "out of memory");
		return;
	}
	new (&kctx->algorithm_cached) Global<Value>();
	new (&kctx->usages_cached) Global<Value>();

	String::Utf8Value algo_name(iso, info[0]);
	if (!*algo_name) {
		free(kctx);
		nx_throw(iso, "expected algorithm name");
		return;
	}
	if (strcmp(*algo_name, "RSA-OAEP") == 0)
		kctx->algorithm = NX_CRYPTO_KEY_ALGORITHM_RSA_OAEP;
	else if (strcmp(*algo_name, "RSA-PSS") == 0)
		kctx->algorithm = NX_CRYPTO_KEY_ALGORITHM_RSA_PSS;
	else if (strcmp(*algo_name, "RSASSA-PKCS1-v1_5") == 0)
		kctx->algorithm = NX_CRYPTO_KEY_ALGORITHM_RSASSA_PKCS1_V1_5;
	else {
		free(kctx);
		nx_throw(iso, "Unsupported RSA algorithm");
		return;
	}

	String::Utf8Value hash_name(iso, info[1]);
	if (!*hash_name) {
		free(kctx);
		nx_throw(iso, "expected hash name");
		return;
	}
	String::Utf8Value type_str(iso, info[2]);
	if (!*type_str) {
		free(kctx);
		nx_throw(iso, "expected type");
		return;
	}
	if (strcmp(*type_str, "public") == 0)
		kctx->type = NX_CRYPTO_KEY_TYPE_PUBLIC;
	else if (strcmp(*type_str, "private") == 0)
		kctx->type = NX_CRYPTO_KEY_TYPE_PRIVATE;
	else {
		free(kctx);
		nx_throw(iso, "Key type must be 'public' or 'private'");
		return;
	}

	kctx->extractable = info[8]->BooleanValue(iso);

	nx_crypto_key_usage usages;
	nx_crypto_parse_usages(iso, info[9], &usages, false);
	kctx->usages = usages;

	nx_crypto_key_rsa_t *rsa =
	    (nx_crypto_key_rsa_t *)calloc(1, sizeof(nx_crypto_key_rsa_t));
	if (!rsa) {
		free(kctx);
		nx_throw(iso, "out of memory");
		return;
	}
	{
		int padding =
		    (kctx->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSASSA_PKCS1_V1_5)
		        ? MBEDTLS_RSA_PKCS_V15
		        : MBEDTLS_RSA_PKCS_V21;
		mbedtls_md_type_t md = nx_crypto_get_md_type(*hash_name);
#if MBEDTLS_VERSION_MAJOR >= 3
		mbedtls_rsa_init(&rsa->rsa);
		mbedtls_rsa_set_padding(&rsa->rsa, padding, md);
#else
		mbedtls_rsa_init(&rsa->rsa, padding, md);
#endif
	}
	strncpy(rsa->hash_name, *hash_name, sizeof(rsa->hash_name) - 1);
	rsa->salt_length = -1;

	size_t n_size = 0, e_size = 0;
	const uint8_t *n_data = NX_GetBufferSource(iso, &n_size, info[3]);
	const uint8_t *e_data = NX_GetBufferSource(iso, &e_size, info[4]);
	if (!n_data || !e_data) {
		mbedtls_rsa_free(&rsa->rsa);
		free(rsa);
		free(kctx);
		nx_throw(iso, "expected n and e");
		return;
	}

	mbedtls_mpi N, E;
	mbedtls_mpi_init(&N);
	mbedtls_mpi_init(&E);
	mbedtls_mpi_read_binary(&N, n_data, n_size);
	mbedtls_mpi_read_binary(&E, e_data, e_size);

	int ret;
	if (kctx->type == NX_CRYPTO_KEY_TYPE_PRIVATE) {
		size_t d_size = 0, p_size = 0, q_size = 0;
		const uint8_t *d_data = NX_GetBufferSource(iso, &d_size, info[5]);
		const uint8_t *p_data = NX_GetBufferSource(iso, &p_size, info[6]);
		const uint8_t *q_data = NX_GetBufferSource(iso, &q_size, info[7]);
		mbedtls_mpi D, P, Q;
		mbedtls_mpi *pD = NULL, *pP = NULL, *pQ = NULL;
		mbedtls_mpi_init(&D);
		mbedtls_mpi_init(&P);
		mbedtls_mpi_init(&Q);
		if (d_data) {
			mbedtls_mpi_read_binary(&D, d_data, d_size);
			pD = &D;
		}
		if (p_data) {
			mbedtls_mpi_read_binary(&P, p_data, p_size);
			pP = &P;
		}
		if (q_data) {
			mbedtls_mpi_read_binary(&Q, q_data, q_size);
			pQ = &Q;
		}
		ret = mbedtls_rsa_import(&rsa->rsa, &N, pP, pQ, pD, &E);
		mbedtls_mpi_free(&D);
		mbedtls_mpi_free(&P);
		mbedtls_mpi_free(&Q);
		if (ret == 0)
			ret = mbedtls_rsa_complete(&rsa->rsa);
		if (ret != 0) {
			mbedtls_mpi_free(&N);
			mbedtls_mpi_free(&E);
			mbedtls_rsa_free(&rsa->rsa);
			free(rsa);
			free(kctx);
			nx_throw(iso, "Failed to import RSA private key");
			return;
		}
	} else {
		ret = mbedtls_rsa_import(&rsa->rsa, &N, NULL, NULL, NULL, &E);
		if (ret == 0)
			ret = mbedtls_rsa_complete(&rsa->rsa);
		if (ret != 0) {
			mbedtls_mpi_free(&N);
			mbedtls_mpi_free(&E);
			mbedtls_rsa_free(&rsa->rsa);
			free(rsa);
			free(kctx);
			nx_throw(iso, "Failed to import RSA public key");
			return;
		}
	}
	mbedtls_mpi_free(&N);
	mbedtls_mpi_free(&E);

	kctx->handle = rsa;
	info.GetReturnValue().Set(nx_make_crypto_key(iso, kctx));
}

// ==================================================================
// RSA export components -> [n,e,d,p,q,dp,dq,qi] (private) or [n,e] (public)
// ==================================================================

void nx_crypto_rsa_export_components(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	nx_crypto_key_t *key = nx_get_crypto_key(info[0]);
	if (!key) {
		nx_throw(iso, "invalid key");
		return;
	}
	if (!key->extractable) {
		nx_throw(iso, "Key is not extractable");
		return;
	}
	nx_crypto_key_rsa_t *rsa = (nx_crypto_key_rsa_t *)key->handle;
	if (!rsa) {
		nx_throw(iso, "Not an RSA key");
		return;
	}
	Local<Array> result = Array::New(iso);

#define EXPORT_MPI_JS(idx, mpi_var)                                            \
	{                                                                          \
		size_t len = mbedtls_mpi_size(&mpi_var);                               \
		uint8_t *buf = (uint8_t *)malloc(len);                                 \
		if (buf) {                                                             \
			mbedtls_mpi_write_binary(&mpi_var, buf, len);                      \
			result->Set(context, idx, nx_ab_take(iso, buf, len)).Check();      \
		}                                                                      \
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
		mbedtls_mpi N, E;
		mbedtls_mpi_init(&N); mbedtls_mpi_init(&E);
		mbedtls_rsa_export(&rsa->rsa, &N, NULL, NULL, NULL, &E);
		EXPORT_MPI_JS(0, N);
		EXPORT_MPI_JS(1, E);
		mbedtls_mpi_free(&N); mbedtls_mpi_free(&E);
	}
#undef EXPORT_MPI_JS
	info.GetReturnValue().Set(result);
}

// ==================================================================
// PKCS8 / SPKI export
// ==================================================================

void nx_crypto_export_key_pkcs8(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_crypto_key_t *key = nx_get_crypto_key(info[0]);
	if (!key) {
		nx_throw(iso, "invalid key");
		return;
	}
	if (!key->extractable) {
		nx_throw(iso, "Key is not extractable");
		return;
	}
	mbedtls_pk_context pk;
	mbedtls_pk_init(&pk);
	int ret;
	if (key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_OAEP ||
	    key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_PSS ||
	    key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSASSA_PKCS1_V1_5) {
		nx_crypto_key_rsa_t *rsa = (nx_crypto_key_rsa_t *)key->handle;
		ret = mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
		if (ret != 0) {
			mbedtls_pk_free(&pk);
			nx_throw(iso, "pk setup failed");
			return;
		}
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
		if (ret != 0) {
			mbedtls_pk_free(&pk);
			nx_throw(iso, "pk setup failed");
			return;
		}
		mbedtls_ecp_keypair *pk_ec = mbedtls_pk_ec(pk);
		mbedtls_ecp_group_load(&pk_ec->grp, ec->keypair.grp.id);
		mbedtls_mpi_copy(&pk_ec->d, &ec->keypair.d);
		mbedtls_ecp_copy(&pk_ec->Q, &ec->keypair.Q);
	} else {
		mbedtls_pk_free(&pk);
		nx_throw(iso, "Key type does not support PKCS8 export");
		return;
	}
	uint8_t buf[4096];
	ret = mbedtls_pk_write_key_der(&pk, buf, sizeof(buf));
	mbedtls_pk_free(&pk);
	if (ret < 0) {
		nx_throw(iso, "Failed to write PKCS8 DER");
		return;
	}
	uint8_t *der_start = buf + sizeof(buf) - ret;
	uint8_t *result = (uint8_t *)malloc(ret);
	if (!result) {
		nx_throw(iso, "out of memory");
		return;
	}
	memcpy(result, der_start, ret);
	info.GetReturnValue().Set(nx_ab_take(iso, result, ret));
}

void nx_crypto_export_key_spki(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_crypto_key_t *key = nx_get_crypto_key(info[0]);
	if (!key) {
		nx_throw(iso, "invalid key");
		return;
	}
	if (!key->extractable) {
		nx_throw(iso, "Key is not extractable");
		return;
	}
	mbedtls_pk_context pk;
	mbedtls_pk_init(&pk);
	int ret;
	if (key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_OAEP ||
	    key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSA_PSS ||
	    key->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSASSA_PKCS1_V1_5) {
		nx_crypto_key_rsa_t *rsa = (nx_crypto_key_rsa_t *)key->handle;
		ret = mbedtls_pk_setup(&pk, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
		if (ret != 0) {
			mbedtls_pk_free(&pk);
			nx_throw(iso, "pk setup failed");
			return;
		}
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
		if (ret != 0) {
			mbedtls_pk_free(&pk);
			nx_throw(iso, "pk setup failed");
			return;
		}
		mbedtls_ecp_keypair *pk_ec = mbedtls_pk_ec(pk);
		mbedtls_ecp_group_load(&pk_ec->grp, ec->keypair.grp.id);
		mbedtls_ecp_copy(&pk_ec->Q, &ec->keypair.Q);
	} else {
		mbedtls_pk_free(&pk);
		nx_throw(iso, "Key type does not support SPKI export");
		return;
	}
	uint8_t buf[4096];
	ret = mbedtls_pk_write_pubkey_der(&pk, buf, sizeof(buf));
	mbedtls_pk_free(&pk);
	if (ret < 0) {
		nx_throw(iso, "Failed to write SPKI DER");
		return;
	}
	uint8_t *der_start = buf + sizeof(buf) - ret;
	uint8_t *result = (uint8_t *)malloc(ret);
	if (!result) {
		nx_throw(iso, "out of memory");
		return;
	}
	memcpy(result, der_start, ret);
	info.GetReturnValue().Set(nx_ab_take(iso, result, ret));
}

// ==================================================================
// PKCS8 / SPKI import
// argv: 0=format, 1=DER, 2=algo name, 3=hash/curve, 4=extractable, 5=usages
// ==================================================================

void nx_crypto_import_key_pkcs8_spki(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();

	String::Utf8Value format(iso, info[0]);
	if (!*format) {
		nx_throw(iso, "expected format");
		return;
	}
	size_t der_size = 0;
	const uint8_t *der_data = NX_GetBufferSource(iso, &der_size, info[1]);
	if (!der_data) {
		nx_throw(iso, "expected DER buffer");
		return;
	}
	String::Utf8Value algo_name(iso, info[2]);
	if (!*algo_name) {
		nx_throw(iso, "expected algorithm name");
		return;
	}
	String::Utf8Value param_name(iso, info[3]); // hash or namedCurve
	if (!*param_name) {
		nx_throw(iso, "expected hash/curve");
		return;
	}
	bool extractable = info[4]->BooleanValue(iso);

	mbedtls_pk_context pk;
	mbedtls_pk_init(&pk);
	int ret;
	bool is_pkcs8 = strcmp(*format, "pkcs8") == 0;
	if (is_pkcs8) {
#if MBEDTLS_VERSION_MAJOR >= 3
		mbedtls_entropy_context imp_entropy;
		mbedtls_ctr_drbg_context imp_ctr_drbg;
		mbedtls_entropy_init(&imp_entropy);
		mbedtls_ctr_drbg_init(&imp_ctr_drbg);
		ret = mbedtls_ctr_drbg_seed(&imp_ctr_drbg, mbedtls_entropy_func,
		                            &imp_entropy, NULL, 0);
		if (ret != 0) {
			mbedtls_ctr_drbg_free(&imp_ctr_drbg);
			mbedtls_entropy_free(&imp_entropy);
			mbedtls_pk_free(&pk);
			nx_throw(iso, "Failed to seed DRBG");
			return;
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
		nx_throw(iso, "Failed to parse key");
		return;
	}

	nx_crypto_key_t *kctx =
	    (nx_crypto_key_t *)calloc(1, sizeof(nx_crypto_key_t));
	if (!kctx) {
		mbedtls_pk_free(&pk);
		nx_throw(iso, "out of memory");
		return;
	}
	new (&kctx->algorithm_cached) Global<Value>();
	new (&kctx->usages_cached) Global<Value>();
	kctx->extractable = extractable;

	nx_crypto_key_usage usages;
	nx_crypto_parse_usages(iso, info[5], &usages, false);
	kctx->usages = usages;
	kctx->type =
	    is_pkcs8 ? NX_CRYPTO_KEY_TYPE_PRIVATE : NX_CRYPTO_KEY_TYPE_PUBLIC;

	mbedtls_pk_type_t pk_type = mbedtls_pk_get_type(&pk);
	if (pk_type == MBEDTLS_PK_RSA) {
		if (strcmp(*algo_name, "RSA-OAEP") == 0)
			kctx->algorithm = NX_CRYPTO_KEY_ALGORITHM_RSA_OAEP;
		else if (strcmp(*algo_name, "RSA-PSS") == 0)
			kctx->algorithm = NX_CRYPTO_KEY_ALGORITHM_RSA_PSS;
		else if (strcmp(*algo_name, "RSASSA-PKCS1-v1_5") == 0)
			kctx->algorithm = NX_CRYPTO_KEY_ALGORITHM_RSASSA_PKCS1_V1_5;
		else {
			mbedtls_pk_free(&pk);
			free(kctx);
			nx_throw(iso, "Unsupported RSA algorithm for import");
			return;
		}
		nx_crypto_key_rsa_t *rsa =
		    (nx_crypto_key_rsa_t *)calloc(1, sizeof(nx_crypto_key_rsa_t));
		if (!rsa) {
			mbedtls_pk_free(&pk);
			free(kctx);
			nx_throw(iso, "out of memory");
			return;
		}
		{
			int padding =
			    (kctx->algorithm == NX_CRYPTO_KEY_ALGORITHM_RSASSA_PKCS1_V1_5)
			        ? MBEDTLS_RSA_PKCS_V15
			        : MBEDTLS_RSA_PKCS_V21;
			mbedtls_md_type_t md = nx_crypto_get_md_type(*param_name);
#if MBEDTLS_VERSION_MAJOR >= 3
			mbedtls_rsa_init(&rsa->rsa);
			mbedtls_rsa_set_padding(&rsa->rsa, padding, md);
#else
			mbedtls_rsa_init(&rsa->rsa, padding, md);
#endif
		}
		strncpy(rsa->hash_name, *param_name, sizeof(rsa->hash_name) - 1);
		rsa->salt_length = -1;
		mbedtls_rsa_context *src_rsa = mbedtls_pk_rsa(pk);
		ret = mbedtls_rsa_copy(&rsa->rsa, src_rsa);
		if (ret != 0) {
			mbedtls_pk_free(&pk);
			mbedtls_rsa_free(&rsa->rsa);
			free(rsa);
			free(kctx);
			nx_throw(iso, "Failed to copy RSA key");
			return;
		}
		kctx->handle = rsa;
	} else if (pk_type == MBEDTLS_PK_ECKEY) {
		if (strcmp(*algo_name, "ECDSA") == 0)
			kctx->algorithm = NX_CRYPTO_KEY_ALGORITHM_ECDSA;
		else if (strcmp(*algo_name, "ECDH") == 0)
			kctx->algorithm = NX_CRYPTO_KEY_ALGORITHM_ECDH;
		else {
			mbedtls_pk_free(&pk);
			free(kctx);
			nx_throw(iso, "Unsupported EC algorithm for import");
			return;
		}
		nx_crypto_key_ec_t *ec =
		    (nx_crypto_key_ec_t *)calloc(1, sizeof(nx_crypto_key_ec_t));
		if (!ec) {
			mbedtls_pk_free(&pk);
			free(kctx);
			nx_throw(iso, "out of memory");
			return;
		}
		mbedtls_ecp_keypair_init(&ec->keypair);
		strncpy(ec->curve_name, *param_name, sizeof(ec->curve_name) - 1);
		mbedtls_ecp_keypair *src_ec = mbedtls_pk_ec(pk);
		mbedtls_ecp_group_load(&ec->keypair.grp, src_ec->grp.id);
		mbedtls_ecp_copy(&ec->keypair.Q, &src_ec->Q);
		if (kctx->type == NX_CRYPTO_KEY_TYPE_PRIVATE)
			mbedtls_mpi_copy(&ec->keypair.d, &src_ec->d);
		kctx->handle = ec;
	} else {
		mbedtls_pk_free(&pk);
		free(kctx);
		nx_throw(iso, "Unsupported key type in DER");
		return;
	}

	mbedtls_pk_free(&pk);
	info.GetReturnValue().Set(nx_make_crypto_key(iso, kctx));
}

// ==================================================================
// EC export public raw (uncompressed point)
// ==================================================================

void nx_crypto_ec_export_public_raw(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_crypto_key_t *key = nx_get_crypto_key(info[0]);
	if (!key) {
		nx_throw(iso, "invalid key");
		return;
	}
	if (key->algorithm != NX_CRYPTO_KEY_ALGORITHM_ECDSA &&
	    key->algorithm != NX_CRYPTO_KEY_ALGORITHM_ECDH) {
		nx_throw(iso, "Not an EC key");
		return;
	}
	if (!key->extractable) {
		nx_throw(iso, "Key is not extractable");
		return;
	}
	nx_crypto_key_ec_t *ec = (nx_crypto_key_ec_t *)key->handle;
	size_t coord_size = mbedtls_mpi_size(&ec->keypair.grp.P);
	size_t buf_size = 1 + 2 * coord_size;
	uint8_t *buf = (uint8_t *)malloc(buf_size);
	if (!buf) {
		nx_throw(iso, "out of memory");
		return;
	}
	size_t olen = 0;
	int ret = mbedtls_ecp_point_write_binary(&ec->keypair.grp, &ec->keypair.Q,
	                                         MBEDTLS_ECP_PF_UNCOMPRESSED, &olen,
	                                         buf, buf_size);
	if (ret != 0) {
		free(buf);
		nx_throw(iso, "Failed to export EC public point");
		return;
	}
	info.GetReturnValue().Set(nx_ab_take(iso, buf, olen));
}

} // namespace

void nx_init_crypto(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "cryptoInit", nx_crypto_init);
	NX_SET_FUNC(init_obj, "cryptoKeyInit", nx_crypto_key_init);
	NX_SET_FUNC(init_obj, "cryptoKeyNew", nx_crypto_key_new);
	NX_SET_FUNC(init_obj, "cryptoDigest", nx_crypto_digest);
	NX_SET_FUNC(init_obj, "cryptoEncrypt", nx_crypto_encrypt);
	NX_SET_FUNC(init_obj, "cryptoDecrypt", nx_crypto_decrypt);
	NX_SET_FUNC(init_obj, "cryptoSign", nx_crypto_sign);
	NX_SET_FUNC(init_obj, "cryptoVerify", nx_crypto_verify);
	NX_SET_FUNC(init_obj, "cryptoExportKey", nx_crypto_export_key);
	NX_SET_FUNC(init_obj, "cryptoGenerateKeyEc", nx_crypto_generate_key_ec);
	NX_SET_FUNC(init_obj, "cryptoKeyNewEcPrivate", nx_crypto_key_new_ec_private);
	NX_SET_FUNC(init_obj, "cryptoDeriveBits", nx_crypto_derive_bits);
	NX_SET_FUNC(init_obj, "cryptoGenerateKeyRsa", nx_crypto_generate_key_rsa);
	NX_SET_FUNC(init_obj, "cryptoKeyNewRsa", nx_crypto_key_new_rsa);
	NX_SET_FUNC(init_obj, "cryptoRsaExportComponents",
	            nx_crypto_rsa_export_components);
	NX_SET_FUNC(init_obj, "cryptoExportKeyPkcs8", nx_crypto_export_key_pkcs8);
	NX_SET_FUNC(init_obj, "cryptoExportKeySpki", nx_crypto_export_key_spki);
	NX_SET_FUNC(init_obj, "cryptoImportKeyPkcs8Spki",
	            nx_crypto_import_key_pkcs8_spki);
	NX_SET_FUNC(init_obj, "cryptoEcExportPublicRaw",
	            nx_crypto_ec_export_public_raw);
	NX_SET_FUNC(init_obj, "sha256Hex", nx_crypto_sha256_hex);
}
