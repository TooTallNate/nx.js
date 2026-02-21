#pragma once
/**
 * Compat switch.h for WebCrypto host builds.
 *
 * Provides libnx type stubs AND real AES/SHA implementations backed by mbedtls,
 * so that crypto.c compiles and runs correctly on the host.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

// Real mbedtls headers (fetched by CMake)
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
#include <mbedtls/aes.h>

typedef uint32_t Result;
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint8_t u8;

#define BIT(n) (1U << (n))
#define R_FAILED(rc) ((rc) != 0)

#define AES_BLOCK_SIZE 16
#define SHA1_HASH_SIZE 20
#define SHA256_HASH_SIZE 32

// ---- SHA-1 / SHA-256 (using mbedtls) ----

static inline void sha1CalculateHash(void *dst, const void *src, size_t size) {
	mbedtls_sha1(src, size, dst);
}

static inline void sha256CalculateHash(void *dst, const void *src, size_t size) {
	mbedtls_sha256(src, size, dst, 0);
}

// ---- randomGet (using /dev/urandom) ----

static inline void randomGet(void *buf, size_t size) {
	FILE *f = fopen("/dev/urandom", "rb");
	if (f) {
		fread(buf, 1, size, f);
		fclose(f);
	}
}

// ---- AES-CBC contexts (backed by mbedtls) ----

typedef struct {
	mbedtls_aes_context aes;
	uint8_t iv[16];
	bool encrypt;
} Aes128CbcContext, Aes192CbcContext, Aes256CbcContext;

static inline void aes128CbcContextCreate(Aes128CbcContext *ctx, const void *key, const void *iv, bool encrypt) {
	mbedtls_aes_init(&ctx->aes);
	if (encrypt)
		mbedtls_aes_setkey_enc(&ctx->aes, key, 128);
	else
		mbedtls_aes_setkey_dec(&ctx->aes, key, 128);
	memcpy(ctx->iv, iv, 16);
	ctx->encrypt = encrypt;
}
static inline void aes192CbcContextCreate(Aes192CbcContext *ctx, const void *key, const void *iv, bool encrypt) {
	mbedtls_aes_init(&ctx->aes);
	if (encrypt)
		mbedtls_aes_setkey_enc(&ctx->aes, key, 192);
	else
		mbedtls_aes_setkey_dec(&ctx->aes, key, 192);
	memcpy(ctx->iv, iv, 16);
	ctx->encrypt = encrypt;
}
static inline void aes256CbcContextCreate(Aes256CbcContext *ctx, const void *key, const void *iv, bool encrypt) {
	mbedtls_aes_init(&ctx->aes);
	if (encrypt)
		mbedtls_aes_setkey_enc(&ctx->aes, key, 256);
	else
		mbedtls_aes_setkey_dec(&ctx->aes, key, 256);
	memcpy(ctx->iv, iv, 16);
	ctx->encrypt = encrypt;
}

static inline void aes128CbcContextResetIv(Aes128CbcContext *ctx, const void *iv) { memcpy(ctx->iv, iv, 16); }
static inline void aes192CbcContextResetIv(Aes192CbcContext *ctx, const void *iv) { memcpy(ctx->iv, iv, 16); }
static inline void aes256CbcContextResetIv(Aes256CbcContext *ctx, const void *iv) { memcpy(ctx->iv, iv, 16); }

static inline void aes128CbcEncrypt(Aes128CbcContext *ctx, void *dst, const void *src, size_t size) {
	uint8_t iv_copy[16]; memcpy(iv_copy, ctx->iv, 16);
	mbedtls_aes_crypt_cbc(&ctx->aes, MBEDTLS_AES_ENCRYPT, size, iv_copy, src, dst);
}
static inline void aes192CbcEncrypt(Aes192CbcContext *ctx, void *dst, const void *src, size_t size) {
	uint8_t iv_copy[16]; memcpy(iv_copy, ctx->iv, 16);
	mbedtls_aes_crypt_cbc(&ctx->aes, MBEDTLS_AES_ENCRYPT, size, iv_copy, src, dst);
}
static inline void aes256CbcEncrypt(Aes256CbcContext *ctx, void *dst, const void *src, size_t size) {
	uint8_t iv_copy[16]; memcpy(iv_copy, ctx->iv, 16);
	mbedtls_aes_crypt_cbc(&ctx->aes, MBEDTLS_AES_ENCRYPT, size, iv_copy, src, dst);
}

static inline void aes128CbcDecrypt(Aes128CbcContext *ctx, void *dst, const void *src, size_t size) {
	uint8_t iv_copy[16]; memcpy(iv_copy, ctx->iv, 16);
	mbedtls_aes_crypt_cbc(&ctx->aes, MBEDTLS_AES_DECRYPT, size, iv_copy, src, dst);
}
static inline void aes192CbcDecrypt(Aes192CbcContext *ctx, void *dst, const void *src, size_t size) {
	uint8_t iv_copy[16]; memcpy(iv_copy, ctx->iv, 16);
	mbedtls_aes_crypt_cbc(&ctx->aes, MBEDTLS_AES_DECRYPT, size, iv_copy, src, dst);
}
static inline void aes256CbcDecrypt(Aes256CbcContext *ctx, void *dst, const void *src, size_t size) {
	uint8_t iv_copy[16]; memcpy(iv_copy, ctx->iv, 16);
	mbedtls_aes_crypt_cbc(&ctx->aes, MBEDTLS_AES_DECRYPT, size, iv_copy, src, dst);
}

// ---- AES-CTR contexts (backed by mbedtls) ----

typedef struct {
	mbedtls_aes_context aes;
	uint8_t ctr[16];
	uint8_t stream_block[16];
	size_t nc_off;
} Aes128CtrContext, Aes192CtrContext, Aes256CtrContext;

static inline void aes128CtrContextCreate(Aes128CtrContext *ctx, const void *key, const void *ctr) {
	mbedtls_aes_init(&ctx->aes);
	mbedtls_aes_setkey_enc(&ctx->aes, key, 128);
	memcpy(ctx->ctr, ctr, 16);
	memset(ctx->stream_block, 0, 16);
	ctx->nc_off = 0;
}
static inline void aes192CtrContextCreate(Aes192CtrContext *ctx, const void *key, const void *ctr) {
	mbedtls_aes_init(&ctx->aes);
	mbedtls_aes_setkey_enc(&ctx->aes, key, 192);
	memcpy(ctx->ctr, ctr, 16);
	memset(ctx->stream_block, 0, 16);
	ctx->nc_off = 0;
}
static inline void aes256CtrContextCreate(Aes256CtrContext *ctx, const void *key, const void *ctr) {
	mbedtls_aes_init(&ctx->aes);
	mbedtls_aes_setkey_enc(&ctx->aes, key, 256);
	memcpy(ctx->ctr, ctr, 16);
	memset(ctx->stream_block, 0, 16);
	ctx->nc_off = 0;
}

static inline void aes128CtrContextResetCtr(Aes128CtrContext *ctx, const void *ctr) {
	memcpy(ctx->ctr, ctr, 16);
	memset(ctx->stream_block, 0, 16);
	ctx->nc_off = 0;
}
static inline void aes192CtrContextResetCtr(Aes192CtrContext *ctx, const void *ctr) {
	memcpy(ctx->ctr, ctr, 16);
	memset(ctx->stream_block, 0, 16);
	ctx->nc_off = 0;
}
static inline void aes256CtrContextResetCtr(Aes256CtrContext *ctx, const void *ctr) {
	memcpy(ctx->ctr, ctr, 16);
	memset(ctx->stream_block, 0, 16);
	ctx->nc_off = 0;
}

static inline void aes128CtrCrypt(Aes128CtrContext *ctx, void *dst, const void *src, size_t size) {
	uint8_t ctr_copy[16]; memcpy(ctr_copy, ctx->ctr, 16);
	uint8_t sb[16]; memcpy(sb, ctx->stream_block, 16);
	size_t nc = ctx->nc_off;
	mbedtls_aes_crypt_ctr(&ctx->aes, size, &nc, ctr_copy, sb, src, dst);
}
static inline void aes192CtrCrypt(Aes192CtrContext *ctx, void *dst, const void *src, size_t size) {
	uint8_t ctr_copy[16]; memcpy(ctr_copy, ctx->ctr, 16);
	uint8_t sb[16]; memcpy(sb, ctx->stream_block, 16);
	size_t nc = ctx->nc_off;
	mbedtls_aes_crypt_ctr(&ctx->aes, size, &nc, ctr_copy, sb, src, dst);
}
static inline void aes256CtrCrypt(Aes256CtrContext *ctx, void *dst, const void *src, size_t size) {
	uint8_t ctr_copy[16]; memcpy(ctr_copy, ctx->ctr, 16);
	uint8_t sb[16]; memcpy(sb, ctx->stream_block, 16);
	size_t nc = ctx->nc_off;
	mbedtls_aes_crypt_ctr(&ctx->aes, size, &nc, ctr_copy, sb, src, dst);
}

// ---- AES-XTS contexts (stub — not standard WebCrypto, skip for now) ----

typedef struct { int _unused; } Aes128XtsContext, Aes192XtsContext, Aes256XtsContext;

static inline void aes128XtsContextCreate(Aes128XtsContext *ctx, const void *key0, const void *key1, bool encrypt) { (void)ctx; (void)key0; (void)key1; (void)encrypt; }
static inline void aes192XtsContextCreate(Aes192XtsContext *ctx, const void *key0, const void *key1, bool encrypt) { (void)ctx; (void)key0; (void)key1; (void)encrypt; }
static inline void aes256XtsContextCreate(Aes256XtsContext *ctx, const void *key0, const void *key1, bool encrypt) { (void)ctx; (void)key0; (void)key1; (void)encrypt; }
static inline void aes128XtsContextResetSector(Aes128XtsContext *ctx, uint64_t sector, bool is_nintendo) { (void)ctx; (void)sector; (void)is_nintendo; }
static inline size_t aes128XtsEncrypt(Aes128XtsContext *ctx, void *dst, const void *src, size_t size) { (void)ctx; (void)dst; (void)src; return size; }
static inline size_t aes128XtsDecrypt(Aes128XtsContext *ctx, void *dst, const void *src, size_t size) { (void)ctx; (void)dst; (void)src; return size; }

// ---- PadState / HidVibrationDeviceHandle (from wasm compat) ----

typedef uint64_t HidVibrationDeviceHandle;
typedef struct { int _unused; } PadState;

// Pl font stubs
typedef enum {
	PlSharedFontType_Standard = 0,
	PlSharedFontType_Total = 6,
} PlSharedFontType;
typedef struct { void *address; uint32_t size; } PlFontData;
static inline Result plGetSharedFontByType(PlFontData *font, PlSharedFontType type) { (void)font; (void)type; return 1; }
