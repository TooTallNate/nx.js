#pragma once
#include "types.h"

typedef struct
{
    HidNpadIdType id;
    PadState *pad;
} nx_gamepad_t;

typedef struct
{
    nx_gamepad_t *gamepad;
    u64 mask;
} nx_gamepad_button_t;

nx_gamepad_t *nx_get_gamepad(JSContext *ctx, JSValueConst obj);

void nx_init_gamepad(JSContext *ctx, JSValueConst init_obj);
