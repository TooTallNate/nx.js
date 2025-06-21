#pragma once
#include "types.h"

u8 *NX_GetBufferSource(JSContext *ctx, size_t *size, JSValue obj);

void replace_file_extension(char *path, const char *new_extension);
