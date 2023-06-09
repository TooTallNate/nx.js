#include "types.h"
#include "wasm.h"

static JSClassID js_wasm_runtime_id;
static JSClassID js_wasm_module_id;

typedef struct {
    IM3Module module;
    bool loaded;
} WasmModule;

static JSValue js_wasm_new_module(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);

    if (nx_ctx->wasm_env == NULL) {
        nx_ctx->wasm_env = m3_NewEnvironment();
    }

    JSValue obj = JS_NewObjectClass(ctx, js_wasm_module_id);
    return obj;
}

static void finalizer_wasm_runtime(JSRuntime *rt, JSValue val)
{
    IM3Runtime runtime = JS_GetOpaque(val, js_wasm_runtime_id);
    if (runtime)
    {
        m3_FreeRuntime(runtime);
    }
}

static void finalizer_wasm_module(JSRuntime *rt, JSValue val)
{
    WasmModule *module = JS_GetOpaque(val, js_wasm_module_id);
    if (module)
    {
        if (!module->loaded) {
            printf("freeing module\n");
            m3_FreeModule(module->module);
        }
        js_free_rt(rt, module);
    }
}

static const JSCFunctionListEntry function_list[] = {
    JS_CFUNC_DEF("wasmNewModule", 1, js_wasm_new_module),
};

void js_wasm_init(JSContext *ctx, JSValueConst native_obj) {
    JSRuntime *rt = JS_GetRuntime(ctx);

    JS_NewClassID(&js_wasm_runtime_id);
    JSClassDef js_wasm_runtime_class = {
        "WebAssembly.Memory",
        .finalizer = finalizer_wasm_runtime,
    };
    JS_NewClass(rt, js_wasm_runtime_id, &js_wasm_runtime_class);

    JS_NewClassID(&js_wasm_module_id);
    JSClassDef js_wasm_module_class = {
        "WebAssembly.Module",
        .finalizer = finalizer_wasm_module,
    };
    JS_NewClass(rt, js_wasm_module_id, &js_wasm_module_class);

    JS_SetPropertyFunctionList(ctx, native_obj, function_list, sizeof(function_list) / sizeof(function_list[0]));
}
