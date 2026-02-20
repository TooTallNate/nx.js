#pragma once
#define MBEDTLS_ALLOW_PRIVATE_ACCESS
#include "types.h"
#include <mbedtls/ecp.h>

typedef enum {
	NX_CRYPTO_KEY_TYPE_UNKNOWN,
	NX_CRYPTO_KEY_TYPE_PRIVATE,
	NX_CRYPTO_KEY_TYPE_PUBLIC,
	NX_CRYPTO_KEY_TYPE_SECRET
} nx_crypto_key_type;

typedef enum {
	NX_CRYPTO_KEY_ALGORITHM_UNKNOWN,
	NX_CRYPTO_KEY_ALGORITHM_AES_CBC,
	NX_CRYPTO_KEY_ALGORITHM_AES_CTR,
	NX_CRYPTO_KEY_ALGORITHM_AES_XTS,
	NX_CRYPTO_KEY_ALGORITHM_AES_GCM,
	NX_CRYPTO_KEY_ALGORITHM_HMAC,
	NX_CRYPTO_KEY_ALGORITHM_ECDSA,
	NX_CRYPTO_KEY_ALGORITHM_ECDH,
	NX_CRYPTO_KEY_ALGORITHM_PBKDF2,
	NX_CRYPTO_KEY_ALGORITHM_HKDF
} nx_crypto_key_algorithm;

typedef enum {
	NX_CRYPTO_KEY_USAGE_DECRYPT = BIT(0),
	NX_CRYPTO_KEY_USAGE_DERIVE_BITS = BIT(1),
	NX_CRYPTO_KEY_USAGE_DERIVE_KEY = BIT(2),
	NX_CRYPTO_KEY_USAGE_ENCRYPT = BIT(3),
	NX_CRYPTO_KEY_USAGE_SIGN = BIT(4),
	NX_CRYPTO_KEY_USAGE_UNWRAP_KEY = BIT(5),
	NX_CRYPTO_KEY_USAGE_VERIFY = BIT(6),
	NX_CRYPTO_KEY_USAGE_WRAP_KEY = BIT(7)
} nx_crypto_key_usage;

typedef struct {
	nx_crypto_key_type type;
	bool extractable;
	nx_crypto_key_algorithm algorithm;
	JSValue algorithm_cached;
	nx_crypto_key_usage usages;
	JSValue usages_cached;
	void *handle;
	uint8_t *raw_key_data;  // Raw key material (for exportKey)
	size_t raw_key_size;     // Size of raw key data
} nx_crypto_key_t;

typedef struct {
	uint8_t *key;
	size_t key_length;
	char hash_name[16];  // "SHA-256", etc.
} nx_crypto_key_hmac_t;

typedef struct {
	u8 key_length; /* 16 (128-bit), 24 (192-bit), 32 (256-bit), etc. */
	union {
		Aes128CbcContext cbc_128;
		Aes192CbcContext cbc_192;
		Aes256CbcContext cbc_256;

		Aes128CtrContext ctr_128;
		Aes192CtrContext ctr_192;
		Aes256CtrContext ctr_256;

		Aes128XtsContext xts_128;
		Aes192XtsContext xts_192;
		Aes256XtsContext xts_256;
	} decrypt;
	union {
		Aes128CbcContext cbc_128;
		Aes192CbcContext cbc_192;
		Aes256CbcContext cbc_256;

		// AES-CTR only needs one context for both encrypt and decrypt,
		// so for encrypt, we use the same context as decrypt

		Aes128XtsContext xts_128;
		Aes192XtsContext xts_192;
		Aes256XtsContext xts_256;
	} encrypt;
} nx_crypto_key_aes_t;

typedef struct {
	mbedtls_ecp_keypair keypair;
	char curve_name[8];  // "P-256" or "P-384"
} nx_crypto_key_ec_t;

void nx_init_crypto(JSContext *ctx, JSValueConst init_obj);
