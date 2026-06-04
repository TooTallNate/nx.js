#pragma once
#define MBEDTLS_ALLOW_PRIVATE_ACCESS
#include "types.h"
#include <mbedtls/ecp.h>
#include <mbedtls/rsa.h>
#include <mbedtls/version.h>

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
	NX_CRYPTO_KEY_ALGORITHM_HKDF,
	NX_CRYPTO_KEY_ALGORITHM_RSA_OAEP,
	NX_CRYPTO_KEY_ALGORITHM_RSA_PSS,
	NX_CRYPTO_KEY_ALGORITHM_RSASSA_PKCS1_V1_5
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
	v8::Global<v8::Value> algorithm_cached;
	nx_crypto_key_usage usages;
	v8::Global<v8::Value> usages_cached;
	void *handle;
	uint8_t *raw_key_data;
	size_t raw_key_size;
} nx_crypto_key_t;

typedef struct {
	uint8_t *key;
	size_t key_length;
	char hash_name[16];
} nx_crypto_key_hmac_t;

typedef struct {
	u8 key_length;
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
		Aes128XtsContext xts_128;
		Aes192XtsContext xts_192;
		Aes256XtsContext xts_256;
	} encrypt;
} nx_crypto_key_aes_t;

typedef struct {
	mbedtls_ecp_keypair keypair;
	char curve_name[8];
} nx_crypto_key_ec_t;

typedef struct {
	mbedtls_rsa_context rsa;
	char hash_name[16];
	int salt_length;
} nx_crypto_key_rsa_t;

void nx_init_crypto(v8::Isolate *iso, v8::Local<v8::Object> init_obj);
