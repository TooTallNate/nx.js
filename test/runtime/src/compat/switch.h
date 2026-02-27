#pragma once
// Stub switch.h for host builds — provides minimal libnx type stubs
// needed by font.c (PlSharedFontType, PlFontData, plGetSharedFontByType)

#include <stdint.h>

typedef uint32_t Result;
typedef uint32_t u32;
typedef uint8_t u8;

#define R_FAILED(rc) ((rc) != 0)
#define R_MODULE(rc) (((rc) >> 9) & 0x1FF)
#define R_DESCRIPTION(rc) ((rc) & 0x1FFF)
#define R_VALUE(rc) (rc)

// Pl (pl:) font service types
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

// HID types referenced in types.h nx_context_t
typedef uint64_t HidVibrationDeviceHandle;
typedef struct { int _unused; } PadState;
