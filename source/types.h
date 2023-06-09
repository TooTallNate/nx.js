#pragma once
#include <stdbool.h>
#include <wasm3.h>
#include <quickjs/quickjs.h>

typedef struct
{
    IM3Environment wasm_env;
} nx_context_t;
