#pragma once
// Stub switch.h for host builds — provides libnx type stubs and
// crypto function implementations using mbedtls for the webcrypto test.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>
#include <mbedtls/aes.h>

typedef uint32_t Result;
typedef uint32_t u32;
typedef uint8_t u8;
typedef uint64_t u64;

#ifndef BIT
#define BIT(n) (1U << (n))
#endif

#define R_FAILED(rc) ((rc) != 0)
#define R_MODULE(rc) (((rc) >> 9) & 0x1FF)
#define R_DESCRIPTION(rc) ((rc) & 0x1FFF)
#define R_VALUE(rc) (rc)

#define SHA1_HASH_SIZE 20
#define SHA256_HASH_SIZE 32
#define AES_BLOCK_SIZE 16

// --- SHA-1 / SHA-256 using mbedtls ---

static inline void sha1CalculateHash(void *dst, const void *src, size_t size) {
	mbedtls_sha1(src, size, dst);
}

static inline void sha256CalculateHash(void *dst, const void *src, size_t size) {
	mbedtls_sha256(src, size, dst, 0);
}

// --- Random ---

static inline void randomGet(void *buf, size_t size) {
	// Use /dev/urandom for host builds
	FILE *f = fopen("/dev/urandom", "rb");
	if (f) {
		fread(buf, 1, size, f);
		fclose(f);
	}
}

// --- AES-CBC contexts using mbedtls ---

typedef struct {
	mbedtls_aes_context aes;
	u8 iv[16];
} Aes128CbcContext, Aes192CbcContext, Aes256CbcContext;

static inline void aes128CbcContextCreate(Aes128CbcContext *ctx, const void *key, const void *iv, int encrypt) {
	mbedtls_aes_init(&ctx->aes);
	if (encrypt)
		mbedtls_aes_setkey_enc(&ctx->aes, key, 128);
	else
		mbedtls_aes_setkey_dec(&ctx->aes, key, 128);
	memcpy(ctx->iv, iv, 16);
}

static inline void aes192CbcContextCreate(Aes192CbcContext *ctx, const void *key, const void *iv, int encrypt) {
	mbedtls_aes_init(&ctx->aes);
	if (encrypt)
		mbedtls_aes_setkey_enc(&ctx->aes, key, 192);
	else
		mbedtls_aes_setkey_dec(&ctx->aes, key, 192);
	memcpy(ctx->iv, iv, 16);
}

static inline void aes256CbcContextCreate(Aes256CbcContext *ctx, const void *key, const void *iv, int encrypt) {
	mbedtls_aes_init(&ctx->aes);
	if (encrypt)
		mbedtls_aes_setkey_enc(&ctx->aes, key, 256);
	else
		mbedtls_aes_setkey_dec(&ctx->aes, key, 256);
	memcpy(ctx->iv, iv, 16);
}

static inline void aes128CbcContextResetIv(Aes128CbcContext *ctx, const void *iv) { memcpy(ctx->iv, iv, 16); }
static inline void aes192CbcContextResetIv(Aes192CbcContext *ctx, const void *iv) { memcpy(ctx->iv, iv, 16); }
static inline void aes256CbcContextResetIv(Aes256CbcContext *ctx, const void *iv) { memcpy(ctx->iv, iv, 16); }

static inline void aes128CbcEncrypt(Aes128CbcContext *ctx, void *dst, const void *src, size_t size) {
	u8 iv[16]; memcpy(iv, ctx->iv, 16);
	mbedtls_aes_crypt_cbc(&ctx->aes, MBEDTLS_AES_ENCRYPT, size, iv, src, dst);
}
static inline void aes192CbcEncrypt(Aes192CbcContext *ctx, void *dst, const void *src, size_t size) {
	u8 iv[16]; memcpy(iv, ctx->iv, 16);
	mbedtls_aes_crypt_cbc(&ctx->aes, MBEDTLS_AES_ENCRYPT, size, iv, src, dst);
}
static inline void aes256CbcEncrypt(Aes256CbcContext *ctx, void *dst, const void *src, size_t size) {
	u8 iv[16]; memcpy(iv, ctx->iv, 16);
	mbedtls_aes_crypt_cbc(&ctx->aes, MBEDTLS_AES_ENCRYPT, size, iv, src, dst);
}

static inline void aes128CbcDecrypt(Aes128CbcContext *ctx, void *dst, const void *src, size_t size) {
	u8 iv[16]; memcpy(iv, ctx->iv, 16);
	mbedtls_aes_crypt_cbc(&ctx->aes, MBEDTLS_AES_DECRYPT, size, iv, src, dst);
}
static inline void aes192CbcDecrypt(Aes192CbcContext *ctx, void *dst, const void *src, size_t size) {
	u8 iv[16]; memcpy(iv, ctx->iv, 16);
	mbedtls_aes_crypt_cbc(&ctx->aes, MBEDTLS_AES_DECRYPT, size, iv, src, dst);
}
static inline void aes256CbcDecrypt(Aes256CbcContext *ctx, void *dst, const void *src, size_t size) {
	u8 iv[16]; memcpy(iv, ctx->iv, 16);
	mbedtls_aes_crypt_cbc(&ctx->aes, MBEDTLS_AES_DECRYPT, size, iv, src, dst);
}

// --- AES-CTR contexts using mbedtls ---

typedef struct {
	mbedtls_aes_context aes;
	u8 ctr[16];
	u8 stream_block[16];
	size_t nc_off;
} Aes128CtrContext, Aes192CtrContext, Aes256CtrContext;

static inline void aes128CtrContextCreate(Aes128CtrContext *ctx, const void *key, const void *ctr) {
	mbedtls_aes_init(&ctx->aes);
	mbedtls_aes_setkey_enc(&ctx->aes, key, 128);
	memcpy(ctx->ctr, ctr, 16);
	ctx->nc_off = 0;
	memset(ctx->stream_block, 0, 16);
}
static inline void aes192CtrContextCreate(Aes192CtrContext *ctx, const void *key, const void *ctr) {
	mbedtls_aes_init(&ctx->aes);
	mbedtls_aes_setkey_enc(&ctx->aes, key, 192);
	memcpy(ctx->ctr, ctr, 16);
	ctx->nc_off = 0;
	memset(ctx->stream_block, 0, 16);
}
static inline void aes256CtrContextCreate(Aes256CtrContext *ctx, const void *key, const void *ctr) {
	mbedtls_aes_init(&ctx->aes);
	mbedtls_aes_setkey_enc(&ctx->aes, key, 256);
	memcpy(ctx->ctr, ctr, 16);
	ctx->nc_off = 0;
	memset(ctx->stream_block, 0, 16);
}

static inline void aes128CtrContextResetCtr(Aes128CtrContext *ctx, const void *ctr) {
	memcpy(ctx->ctr, ctr, 16); ctx->nc_off = 0; memset(ctx->stream_block, 0, 16);
}
static inline void aes192CtrContextResetCtr(Aes192CtrContext *ctx, const void *ctr) {
	memcpy(ctx->ctr, ctr, 16); ctx->nc_off = 0; memset(ctx->stream_block, 0, 16);
}
static inline void aes256CtrContextResetCtr(Aes256CtrContext *ctx, const void *ctr) {
	memcpy(ctx->ctr, ctr, 16); ctx->nc_off = 0; memset(ctx->stream_block, 0, 16);
}

static inline void aes128CtrCrypt(Aes128CtrContext *ctx, void *dst, const void *src, size_t size) {
	mbedtls_aes_crypt_ctr(&ctx->aes, size, &ctx->nc_off, ctx->ctr, ctx->stream_block, src, dst);
}
static inline void aes192CtrCrypt(Aes192CtrContext *ctx, void *dst, const void *src, size_t size) {
	mbedtls_aes_crypt_ctr(&ctx->aes, size, &ctx->nc_off, ctx->ctr, ctx->stream_block, src, dst);
}
static inline void aes256CtrCrypt(Aes256CtrContext *ctx, void *dst, const void *src, size_t size) {
	mbedtls_aes_crypt_ctr(&ctx->aes, size, &ctx->nc_off, ctx->ctr, ctx->stream_block, src, dst);
}

// --- AES-XTS contexts (stub — not standard WebCrypto, skip for now) ---

typedef struct { int _unused; } Aes128XtsContext, Aes192XtsContext, Aes256XtsContext;

static inline void aes128XtsContextCreate(Aes128XtsContext *ctx, const void *key1, const void *key2, int encrypt) { (void)ctx; (void)key1; (void)key2; (void)encrypt; }
static inline void aes192XtsContextCreate(Aes192XtsContext *ctx, const void *key1, const void *key2, int encrypt) { (void)ctx; (void)key1; (void)key2; (void)encrypt; }
static inline void aes256XtsContextCreate(Aes256XtsContext *ctx, const void *key1, const void *key2, int encrypt) { (void)ctx; (void)key1; (void)key2; (void)encrypt; }
static inline void aes128XtsContextResetSector(Aes128XtsContext *ctx, u64 sector, int is_nintendo) { (void)ctx; (void)sector; (void)is_nintendo; }
static inline size_t aes128XtsEncrypt(Aes128XtsContext *ctx, void *dst, const void *src, size_t size) { (void)ctx; (void)dst; (void)src; return size; }
static inline size_t aes128XtsDecrypt(Aes128XtsContext *ctx, void *dst, const void *src, size_t size) { (void)ctx; (void)dst; (void)src; return size; }

// --- Pl font types (needed by types.h) ---

typedef enum {
	PlSharedFontType_Standard = 0,
	PlSharedFontType_ChineseSimplified = 1,
	PlSharedFontType_ExtChineseSimplified = 2,
	PlSharedFontType_ChineseTraditional = 3,
	PlSharedFontType_KO = 4,
	PlSharedFontType_NintendoExt = 5,
	PlSharedFontType_Total = 6,
} PlSharedFontType;

typedef struct {
	void *address;
	uint32_t size;
} PlFontData;

static inline Result plGetSharedFontByType(PlFontData *font, PlSharedFontType type) {
	(void)font; (void)type; return 1;
}

// HID types referenced in types.h
typedef uint64_t HidVibrationDeviceHandle;
typedef struct { int _unused; } PadState;
