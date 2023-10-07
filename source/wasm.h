#pragma once
#include "types.h"

// `Switch.native`
void nx_init_wasm_(JSContext *ctx, JSValueConst native_obj);

// `$`
void nx_init_wasm(JSContext *ctx, JSValueConst init_obj);
