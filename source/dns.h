#pragma once
#include <switch.h>
#include <quickjs/quickjs.h>
#include "types.h"

typedef struct
{
    int err;
    const char *hostname;
    char **entries;
    size_t num_entries;
} nx_dns_resolve_t;

JSValue js_dns_resolve(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);
