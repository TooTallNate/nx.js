#pragma once
#include <quickjs/quickjs.h>
#include <cairo-ft.h>
#include <ft2build.h>

typedef struct
{
    FT_Library ft_library;
} nx_context_t;

inline nx_context_t *nx_get_context(JSContext *ctx)
{
    return JS_GetContextOpaque(ctx);
}
