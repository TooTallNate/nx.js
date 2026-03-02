#pragma once
/**
 * Compat switch.h for nxjs-test host builds.
 *
 * Provides libnx type stubs AND real AES/SHA implementations backed by mbedtls,
 * so that the nx.js source files compile and run correctly on the host.
 *
 * This is a comprehensive replacement for <switch.h> that covers all libnx
 * symbols used by the compiled source files.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Real mbedtls headers (fetched by CMake)
#include <mbedtls/aes.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha1.h>
#include <mbedtls/sha256.h>

// ============================================================================
// macOS compat: struct stat uses st_mtimespec instead of st_mtim
// ============================================================================

#ifdef __APPLE__
#define st_mtim st_mtimespec
#define st_atim st_atimespec
#define st_ctim st_ctimespec
#endif

// ============================================================================
// Fundamental types (from libnx <switch/types.h>)
// ============================================================================

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef uint32_t Result;

// ============================================================================
// Result handling macros
// ============================================================================

#define R_FAILED(rc) ((rc) != 0)
#define R_SUCCEEDED(rc) ((rc) == 0)
#define R_MODULE(rc) (((rc) >> 9) & 0x1FF)
#define R_DESCRIPTION(rc) ((rc) & 0x1FFF)
#define R_VALUE(rc) (rc)
#define MAKERESULT(module, description) \
	((((module)&0x1FF) << 9) | ((description)&0x1FFF))

#define BIT(n) (1U << (n))

// libnx error codes used by web.c
#define Module_Libnx 345
#define LibnxError_OutOfMemory 2

// ============================================================================
// HOS version macros (used by main.c)
// ============================================================================

#define HOSVER_MAJOR(v) (((v) >> 16) & 0xFF)
#define HOSVER_MINOR(v) (((v) >> 8) & 0xFF)
#define HOSVER_MICRO(v) ((v)&0xFF)

static inline u32 hosversionGet(void) {
	return 0; // 0.0.0
}

static inline bool hosversionIsAtmosphere(void) {
	return false;
}

// ============================================================================
// HID types (referenced in types.h nx_context_t)
// ============================================================================

typedef uint64_t HidVibrationDeviceHandle;
typedef struct {
	int _unused;
} PadState;

// Pad button constants
typedef enum {
	HidNpadButton_B = BIT(0),
	HidNpadButton_A = BIT(1),
	HidNpadButton_Y = BIT(2),
	HidNpadButton_X = BIT(3),
	HidNpadButton_L = BIT(4),
	HidNpadButton_R = BIT(5),
	HidNpadButton_ZL = BIT(6),
	HidNpadButton_ZR = BIT(7),
	HidNpadButton_Minus = BIT(8),
	HidNpadButton_Plus = BIT(9),
	HidNpadButton_StickL = BIT(10),
	HidNpadButton_StickR = BIT(11),
	HidNpadButton_Up = BIT(12),
	HidNpadButton_Down = BIT(13),
	HidNpadButton_Left = BIT(14),
	HidNpadButton_Right = BIT(15),
} HidNpadButton;

typedef enum {
	HidNpadStyleSet_NpadStandard = 0,
	HidNpadStyleTag_NpadGc = 0,
} HidNpadStyleTag;

typedef enum {
	HidNpadIdType_No1 = 0,
	HidNpadIdType_No2 = 1,
	HidNpadIdType_No3 = 2,
	HidNpadIdType_No4 = 3,
	HidNpadIdType_No5 = 4,
	HidNpadIdType_No6 = 5,
	HidNpadIdType_No7 = 6,
	HidNpadIdType_No8 = 7,
	HidNpadIdType_Handheld = 0x20,
} HidNpadIdType;

typedef struct {
	s32 x;
	s32 y;
} HidAnalogStickState;

typedef struct {
	u32 finger_id;
	s32 x;
	s32 y;
	s32 diameter_x;
	s32 diameter_y;
	s32 rotation_angle;
} HidTouchState;

typedef struct {
	u64 sampling_number;
	s32 count;
	HidTouchState touches[16];
} HidTouchScreenState;

typedef struct {
	u64 sampling_number;
	u64 modifiers;
	u64 keys[4];
} HidKeyboardState;

typedef struct {
	float freq_low;
	float amp_low;
	float freq_high;
	float amp_high;
} HidVibrationValue;

// Pad functions (no-ops on host)
static inline void padConfigureInput(int max_pads, int style_set) {
	(void)max_pads;
	(void)style_set;
}
static inline void padInitializeDefault(PadState *pad) {
	(void)pad;
}
static inline void padInitialize(PadState *pad, int id) {
	(void)pad;
	(void)id;
}
static inline void padUpdate(PadState *pad) {
	(void)pad;
}
static inline u64 padGetButtonsDown(PadState *pad) {
	(void)pad;
	return 0;
}
static inline u64 padGetButtons(PadState *pad) {
	(void)pad;
	return 0;
}
static inline HidAnalogStickState padGetStickPos(PadState *pad, int stick) {
	(void)pad;
	(void)stick;
	HidAnalogStickState s = {0, 0};
	return s;
}
static inline bool padIsConnected(PadState *pad) {
	(void)pad;
	return false;
}
static inline u32 padGetStyleSet(PadState *pad) {
	(void)pad;
	return 0;
}

// HID functions (no-ops on host)
static inline void hidInitializeTouchScreen(void) {}
static inline void hidInitializeKeyboard(void) {}
static inline Result hidInitializeVibrationDevices(
    HidVibrationDeviceHandle *handles, int count, int id, int style_set) {
	(void)handles;
	(void)count;
	(void)id;
	(void)style_set;
	return 0;
}
static inline Result hidSendVibrationValues(
    HidVibrationDeviceHandle *handles, HidVibrationValue *values, int count) {
	(void)handles;
	(void)values;
	(void)count;
	return 0;
}
static inline void hidGetTouchScreenStates(HidTouchScreenState *state,
                                           int count) {
	(void)count;
	memset(state, 0, sizeof(HidTouchScreenState));
}
static inline void hidGetKeyboardStates(HidKeyboardState *state, int count) {
	(void)count;
	memset(state, 0, sizeof(HidKeyboardState));
}
static inline u32 hidGetNpadDeviceType(int id) {
	(void)id;
	return 0;
}

// ============================================================================
// Pl (pl:) font service types
// ============================================================================

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

// Stub — always fails on host
static inline Result plGetSharedFontByType(PlFontData *font,
                                           PlSharedFontType type) {
	(void)font;
	(void)type;
	return 1; // R_FAILED
}

// ============================================================================
// Filesystem types (used by fs.c)
// ============================================================================

typedef struct {
	int _unused;
} FsFileSystem;

typedef enum {
	FsCreateOption_BigFile = BIT(0),
} FsCreateOption;

static inline FsFileSystem *fsdevGetDeviceFileSystem(const char *name) {
	(void)name;
	return NULL;
}

static inline Result fsFsCreateFile(FsFileSystem *fs, const char *path,
                                    s64 size, u32 option) {
	(void)fs;
	(void)path;
	(void)size;
	(void)option;
	return 1; // R_FAILED — not available on host
}

// ============================================================================
// Applet types (used by web.c and main.c)
// ============================================================================

typedef enum {
	AppletType_Application = 0,
	AppletType_SystemApplication = 1,
} AppletType;

static inline AppletType appletGetAppletType(void) {
	return AppletType_Application;
}

static inline bool appletMainLoop(void) {
	// This should be overridden by main.c
	return true;
}

// ============================================================================
// Web applet types and functions (used by web.c)
// All stubbed — web applet is not available on host
// ============================================================================

typedef struct {
	int _unused;
} WebCommonConfig;
typedef struct {
	int _unused;
} WebSession;
typedef struct {
	int _unused;
} WebCommonReply;
typedef struct {
	int _unused;
} Event;

typedef enum {
	WebDocumentKind_OfflineHtmlPage = 0,
} WebDocumentKind;

typedef enum {
	WebSessionBootMode_AllForegroundInitiallyHidden = 0,
} WebSessionBootMode;

// All web config functions are no-ops
static inline Result webPageCreate(WebCommonConfig *config, const char *url) {
	(void)config;
	(void)url;
	return 1;
}
static inline Result webOfflineCreate(WebCommonConfig *config, int kind,
                                      u64 id, const char *doc_path) {
	(void)config;
	(void)kind;
	(void)id;
	(void)doc_path;
	return 1;
}

#define STUB_WEB_CONFIG_BOOL(name)                                             \
	static inline Result name(WebCommonConfig *config, bool value) {           \
		(void)config;                                                          \
		(void)value;                                                           \
		return 0;                                                              \
	}
#define STUB_WEB_CONFIG_FLOAT(name)                                            \
	static inline Result name(WebCommonConfig *config, float value) {          \
		(void)config;                                                          \
		(void)value;                                                           \
		return 0;                                                              \
	}
#define STUB_WEB_CONFIG_U32(name)                                              \
	static inline Result name(WebCommonConfig *config, u32 value) {            \
		(void)config;                                                          \
		(void)value;                                                           \
		return 0;                                                              \
	}
#define STUB_WEB_CONFIG_STR(name)                                              \
	static inline Result name(WebCommonConfig *config, const char *value) {    \
		(void)config;                                                          \
		(void)value;                                                           \
		return 0;                                                              \
	}

STUB_WEB_CONFIG_BOOL(webConfigSetJsExtension)
STUB_WEB_CONFIG_U32(webConfigSetBootMode)
STUB_WEB_CONFIG_U32(webConfigSetBootDisplayKind)
STUB_WEB_CONFIG_U32(webConfigSetBackgroundKind)
STUB_WEB_CONFIG_BOOL(webConfigSetFooter)
STUB_WEB_CONFIG_BOOL(webConfigSetPointer)
STUB_WEB_CONFIG_U32(webConfigSetLeftStickMode)
STUB_WEB_CONFIG_BOOL(webConfigSetBootAsMediaPlayer)
STUB_WEB_CONFIG_BOOL(webConfigSetScreenShot)
STUB_WEB_CONFIG_BOOL(webConfigSetPageCache)
STUB_WEB_CONFIG_BOOL(webConfigSetWebAudio)
STUB_WEB_CONFIG_U32(webConfigSetFooterFixedKind)
STUB_WEB_CONFIG_BOOL(webConfigSetPageFade)
STUB_WEB_CONFIG_BOOL(webConfigSetBootLoadingIcon)
STUB_WEB_CONFIG_BOOL(webConfigSetPageScrollIndicator)
STUB_WEB_CONFIG_BOOL(webConfigSetMediaPlayerSpeedControl)
STUB_WEB_CONFIG_BOOL(webConfigSetMediaAutoPlay)
STUB_WEB_CONFIG_FLOAT(webConfigSetOverrideWebAudioVolume)
STUB_WEB_CONFIG_FLOAT(webConfigSetOverrideMediaAudioVolume)
STUB_WEB_CONFIG_BOOL(webConfigSetMediaPlayerAutoClose)
STUB_WEB_CONFIG_BOOL(webConfigSetMediaPlayerUi)
STUB_WEB_CONFIG_STR(webConfigSetUserAgentAdditionalString)
STUB_WEB_CONFIG_BOOL(webConfigSetTouchEnabledOnContents)
STUB_WEB_CONFIG_STR(webConfigSetWhitelist)

static inline Result webSessionCreate(WebSession *session,
                                      WebCommonConfig *config) {
	(void)session;
	(void)config;
	return 1;
}
static inline Result webSessionStart(WebSession *session,
                                     Event **exit_event) {
	(void)session;
	(void)exit_event;
	return 1;
}
static inline void webSessionClose(WebSession *session) {
	(void)session;
}
static inline Result webSessionWaitForExit(WebSession *session,
                                           WebCommonReply *reply) {
	(void)session;
	(void)reply;
	return 1;
}
static inline Result webSessionAppear(WebSession *session,
                                      WebCommonConfig *config) {
	(void)session;
	(void)config;
	return 1;
}
static inline Result webSessionTrySendContentMessage(WebSession *session,
                                                     const char *content,
                                                     u32 size, bool *flag) {
	(void)session;
	(void)content;
	(void)size;
	(void)flag;
	return 1;
}
static inline Result webSessionTryReceiveContentMessage(WebSession *session,
                                                        char *content,
                                                        u64 size,
                                                        u64 *out_size,
                                                        bool *flag) {
	(void)session;
	(void)content;
	(void)size;
	(void)out_size;
	(void)flag;
	return 1;
}
static inline Result webSessionRequestExit(WebSession *session) {
	(void)session;
	return 1;
}
static inline bool eventWait(Event *event, u64 timeout) {
	(void)event;
	(void)timeout;
	return false;
}

// ============================================================================
// SHA-1 / SHA-256 (real implementations via mbedtls)
// ============================================================================

#define SHA1_HASH_SIZE 20
#define SHA256_HASH_SIZE 32
#define AES_BLOCK_SIZE 16

static inline void sha1CalculateHash(void *dst, const void *src, size_t size) {
	mbedtls_sha1(src, size, dst);
}

static inline void sha256CalculateHash(void *dst, const void *src,
                                       size_t size) {
	mbedtls_sha256(src, size, dst, 0);
}

// ============================================================================
// randomGet (using /dev/urandom on host)
// ============================================================================

static inline void randomGet(void *buf, size_t size) {
	FILE *f = fopen("/dev/urandom", "rb");
	if (f) {
		fread(buf, 1, size, f);
		fclose(f);
	}
}

// ============================================================================
// AES-CBC contexts (real implementations via mbedtls)
// ============================================================================

typedef struct {
	mbedtls_aes_context aes;
	uint8_t iv[16];
	bool encrypt;
} Aes128CbcContext, Aes192CbcContext, Aes256CbcContext;

#define IMPL_AES_CBC(bits)                                                     \
	static inline void aes##bits##CbcContextCreate(                            \
	    Aes##bits##CbcContext *ctx, const void *key, const void *iv,           \
	    bool encrypt) {                                                        \
		mbedtls_aes_init(&ctx->aes);                                           \
		if (encrypt)                                                           \
			mbedtls_aes_setkey_enc(&ctx->aes, key, bits);                      \
		else                                                                   \
			mbedtls_aes_setkey_dec(&ctx->aes, key, bits);                      \
		memcpy(ctx->iv, iv, 16);                                               \
		ctx->encrypt = encrypt;                                                \
	}                                                                          \
	static inline void aes##bits##CbcContextResetIv(                           \
	    Aes##bits##CbcContext *ctx, const void *iv) {                           \
		memcpy(ctx->iv, iv, 16);                                               \
	}                                                                          \
	static inline void aes##bits##CbcEncrypt(Aes##bits##CbcContext *ctx,       \
	                                         void *dst, const void *src,       \
	                                         size_t size) {                     \
		uint8_t iv_copy[16];                                                   \
		memcpy(iv_copy, ctx->iv, 16);                                          \
		mbedtls_aes_crypt_cbc(&ctx->aes, MBEDTLS_AES_ENCRYPT, size, iv_copy,  \
		                      src, dst);                                        \
	}                                                                          \
	static inline void aes##bits##CbcDecrypt(Aes##bits##CbcContext *ctx,       \
	                                         void *dst, const void *src,       \
	                                         size_t size) {                     \
		uint8_t iv_copy[16];                                                   \
		memcpy(iv_copy, ctx->iv, 16);                                          \
		mbedtls_aes_crypt_cbc(&ctx->aes, MBEDTLS_AES_DECRYPT, size, iv_copy,  \
		                      src, dst);                                        \
	}

IMPL_AES_CBC(128)
IMPL_AES_CBC(192)
IMPL_AES_CBC(256)

// ============================================================================
// AES-CTR contexts (real implementations via mbedtls)
// ============================================================================

typedef struct {
	mbedtls_aes_context aes;
	uint8_t ctr[16];
	uint8_t stream_block[16];
	size_t nc_off;
} Aes128CtrContext, Aes192CtrContext, Aes256CtrContext;

#define IMPL_AES_CTR(bits)                                                     \
	static inline void aes##bits##CtrContextCreate(                            \
	    Aes##bits##CtrContext *ctx, const void *key, const void *ctr) {        \
		mbedtls_aes_init(&ctx->aes);                                           \
		mbedtls_aes_setkey_enc(&ctx->aes, key, bits);                          \
		memcpy(ctx->ctr, ctr, 16);                                             \
		memset(ctx->stream_block, 0, 16);                                      \
		ctx->nc_off = 0;                                                       \
	}                                                                          \
	static inline void aes##bits##CtrContextResetCtr(                          \
	    Aes##bits##CtrContext *ctx, const void *ctr) {                          \
		memcpy(ctx->ctr, ctr, 16);                                             \
		memset(ctx->stream_block, 0, 16);                                      \
		ctx->nc_off = 0;                                                       \
	}                                                                          \
	static inline void aes##bits##CtrCrypt(Aes##bits##CtrContext *ctx,         \
	                                       void *dst, const void *src,         \
	                                       size_t size) {                       \
		uint8_t ctr_copy[16];                                                  \
		memcpy(ctr_copy, ctx->ctr, 16);                                        \
		uint8_t sb[16];                                                        \
		memcpy(sb, ctx->stream_block, 16);                                     \
		size_t nc = ctx->nc_off;                                               \
		mbedtls_aes_crypt_ctr(&ctx->aes, size, &nc, ctr_copy, sb, src, dst);  \
	}

IMPL_AES_CTR(128)
IMPL_AES_CTR(192)
IMPL_AES_CTR(256)

// ============================================================================
// AES-XTS contexts (stub — not standard WebCrypto)
// ============================================================================

typedef struct {
	int _unused;
} Aes128XtsContext, Aes192XtsContext, Aes256XtsContext;

static inline void aes128XtsContextCreate(Aes128XtsContext *ctx,
                                          const void *key0, const void *key1,
                                          bool encrypt) {
	(void)ctx;
	(void)key0;
	(void)key1;
	(void)encrypt;
}
static inline void aes192XtsContextCreate(Aes192XtsContext *ctx,
                                          const void *key0, const void *key1,
                                          bool encrypt) {
	(void)ctx;
	(void)key0;
	(void)key1;
	(void)encrypt;
}
static inline void aes256XtsContextCreate(Aes256XtsContext *ctx,
                                          const void *key0, const void *key1,
                                          bool encrypt) {
	(void)ctx;
	(void)key0;
	(void)key1;
	(void)encrypt;
}
static inline void aes128XtsContextResetSector(Aes128XtsContext *ctx,
                                               uint64_t sector,
                                               bool is_nintendo) {
	(void)ctx;
	(void)sector;
	(void)is_nintendo;
}
static inline size_t aes128XtsEncrypt(Aes128XtsContext *ctx, void *dst,
                                      const void *src, size_t size) {
	(void)ctx;
	(void)dst;
	(void)src;
	return size;
}
static inline size_t aes128XtsDecrypt(Aes128XtsContext *ctx, void *dst,
                                      const void *src, size_t size) {
	(void)ctx;
	(void)dst;
	(void)src;
	return size;
}

// ============================================================================
// SPL service (used by main.c version getters)
// ============================================================================

typedef enum {
	SplConfigItem_ExosphereApiVersion = 65000,
	SplConfigItem_ExosphereEmummcType = 65007,
} SplConfigItem;

static inline Result splInitialize(void) {
	return 1;
}
static inline void splExit(void) {}
static inline Result splGetConfig(SplConfigItem item, u64 *out) {
	(void)item;
	(void)out;
	return 1;
}

// ============================================================================
// Socket / Network init (used by main.c)
// ============================================================================

typedef enum {
	BsdServiceType_Auto = 0,
} BsdServiceType;

typedef struct {
	u32 tcp_tx_buf_size;
	u32 tcp_rx_buf_size;
	u32 tcp_tx_buf_max_size;
	u32 tcp_rx_buf_max_size;
	u32 udp_tx_buf_size;
	u32 udp_rx_buf_size;
	u32 sb_efficiency;
	u32 num_bsd_sessions;
	BsdServiceType bsd_service_type;
} SocketInitConfig;

static inline Result socketInitialize(const SocketInitConfig *config) {
	(void)config;
	return 0;
}
static inline void socketExit(void) {}

// ============================================================================
// RomFS (used by main.c)
// ============================================================================

static inline Result romfsInit(void) {
	return 0;
}
static inline void romfsExit(void) {}

// ============================================================================
// Pl service init/exit (used by main.c)
// ============================================================================

typedef enum {
	PlServiceType_User = 0,
} PlServiceType;

static inline Result plInitialize(PlServiceType type) {
	(void)type;
	return 0;
}
static inline void plExit(void) {}

// ============================================================================
// Console (used by main.c)
// ============================================================================

typedef struct {
	int _unused;
} PrintConsole;

static inline PrintConsole *consoleInit(PrintConsole *con) {
	(void)con;
	return NULL;
}
static inline void consoleUpdate(PrintConsole *con) {
	(void)con;
}
static inline void consoleExit(PrintConsole *con) {
	(void)con;
}

// ============================================================================
// Framebuffer / NWindow (used by main.c)
// ============================================================================

typedef enum {
	PIXEL_FORMAT_BGRA_8888 = 0,
} PixelFormat;

typedef struct {
	int _unused;
} NWindow;

typedef struct {
	int _unused;
} Framebuffer;

static inline NWindow *nwindowGetDefault(void) {
	return NULL;
}
static inline Result framebufferCreate(Framebuffer *fb, NWindow *win, u32 w,
                                       u32 h, u32 format, u32 num_buffers) {
	(void)fb;
	(void)win;
	(void)w;
	(void)h;
	(void)format;
	(void)num_buffers;
	return 0;
}
static inline Result framebufferMakeLinear(Framebuffer *fb) {
	(void)fb;
	return 0;
}
static inline void *framebufferBegin(Framebuffer *fb, u32 *stride) {
	(void)fb;
	if (stride)
		*stride = 0;
	return NULL;
}
static inline void framebufferEnd(Framebuffer *fb) {
	(void)fb;
}
static inline void framebufferClose(Framebuffer *fb) {
	(void)fb;
}

// ============================================================================
// diagAbort (used by main.c for fatal errors)
// ============================================================================

static inline void diagAbortWithResult(Result rc) {
	fprintf(stderr, "diagAbortWithResult: 0x%x\n", rc);
	abort();
}

// ============================================================================
// Audio types (stubs for types.h / audio.c — module is stubbed)
// ============================================================================

typedef struct {
	int _unused;
} AudioDriver;

// ============================================================================
// NACP types (used by ns.h nx_app_t struct — module is stubbed)
// ============================================================================

// NacpStruct is ~0x4000 bytes in libnx. We only need a minimal stub
// since ns.c is not compiled — only ns.h references it in a struct def.
typedef struct {
	char _padding[0x4000];
} NacpStruct;

// ============================================================================
// Account types (stubs — module is stubbed)
// ============================================================================

typedef struct {
	u64 uid[2];
} AccountUid;

// ============================================================================
// Misc stubs for types/functions that may be referenced transitively
// ============================================================================

// TurboJPEG decode helper (used by main.c nx_render_loading_image)
// The actual decode_jpeg function is defined in util.c
