#pragma once
#include "types.h"

typedef enum {
	NX_CRYPTO_KEY_TYPE_UNKNOWN,
	NX_CRYPTO_KEY_TYPE_PRIVATE,
	NX_CRYPTO_KEY_TYPE_PUBLIC,
	NX_CRYPTO_KEY_TYPE_SECRET
} nx_crypto_key_type;

typedef enum {
	NX_CRYPTO_KEY_ALGORITHM_UNKNOWN,
	NX_CRYPTO_KEY_ALGORITHM_AES_CBC,
	NX_CRYPTO_KEY_ALGORITHM_AES_XTS
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
} nx_crypto_key_t;

typedef struct {
	u8 key_length; /* 16 (128-bit), 24 (192-bit), or 32 (256-bit) */
	union {
		Aes128CbcContext cbc_128;
		Aes192CbcContext cbc_192;
		Aes256CbcContext cbc_256;

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

void nx_init_crypto(JSContext *ctx, JSValueConst init_obj);
