/**
 * Modified from `txiki.js` by Saúl Ibarra Corretgé <s@saghul.net>
 *  - https://github.com/saghul/txiki.js/blob/master/src/wasm.c
 *  - https://github.com/saghul/txiki.js/blob/master/src/js/polyfills/wasm.js
 */
#include "types.h"
#include "wasm.h"
#include <m3_env.h>

JSValue nx_throw_wasm_error(JSContext *ctx, const char *name, M3Result r)
{
    // CHECK_NOT_NULL(r);
    JSValue obj = JS_NewError(ctx);
    JS_DefinePropertyValueStr(ctx, obj, "message", JS_NewString(ctx, r), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    JS_DefinePropertyValueStr(ctx, obj, "wasmError", JS_NewString(ctx, name), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
    if (JS_IsException(obj))
        obj = JS_NULL;
    return JS_Throw(ctx, obj);
}

static JSClassID nx_wasm_module_class_id;

typedef struct
{
    IM3Module module;
    bool loaded;
} nx_wasm_module_t;

static JSValue nx_wasm_new_module(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);

    if (nx_ctx->wasm_env == NULL)
    {
        nx_ctx->wasm_env = m3_NewEnvironment();
    }

    JSValue obj = JS_NewObjectClass(ctx, nx_wasm_module_class_id);
    nx_wasm_module_t *m = js_mallocz(ctx, sizeof(nx_wasm_module_t));
    JS_SetOpaque(obj, m);

    size_t size;
    uint8_t *buf = JS_GetArrayBuffer(ctx, &size, argv[0]);
    // TODO: error handling

    M3Result r = m3_ParseModule(nx_ctx->wasm_env, &m->module, buf, size);
    if (r)
    {
        JS_FreeValue(ctx, obj);
        return nx_throw_wasm_error(ctx, "CompileError", r);
    }

    return obj;
}

static void finalizer_wasm_module(JSRuntime *rt, JSValue val)
{
    nx_wasm_module_t *m = JS_GetOpaque(val, nx_wasm_module_class_id);
    if (m)
    {
        if (!m->loaded)
        {
            // printf("freeing module\n");
            m3_FreeModule(m->module);
        }
        js_free_rt(rt, m);
    }
}

// static JSClassID nx_wasm_instance_class_id;
//
// typedef struct {
//     IM3Runtime runtime;
//     IM3Module module;
//     bool loaded;
// } nx_wasm_instance_t;

static JSValue nx_wasm_module_exports(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_wasm_module_t *m = JS_GetOpaque2(ctx, argv[0], nx_wasm_module_class_id);
    if (!m)
        return JS_EXCEPTION;

    JSValue exports = JS_NewArray(ctx);
    if (JS_IsException(exports))
        return exports;

    for (size_t i = 0, j = 0; i < m->module->numFunctions; ++i)
    {
        IM3Function f = &m->module->functions[i];
        if (f->export_name)
        {
            JSValue item = JS_NewObject(ctx);
            JS_DefinePropertyValueStr(ctx, item, "kind", JS_NewString(ctx, "function"), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, item, "name", JS_NewString(ctx, f->export_name), JS_PROP_C_W_E);
            JS_DefinePropertyValueUint32(ctx, exports, j, item, JS_PROP_C_W_E);
            j++;
        }
    }

    // TODO: other export types.

    return exports;
}

static JSValue nx_wasm_module_imports(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_wasm_module_t *m = JS_GetOpaque2(ctx, argv[0], nx_wasm_module_class_id);
    if (!m)
        return JS_EXCEPTION;

    JSValue imports = JS_NewArray(ctx);
    if (JS_IsException(imports))
        return imports;

    for (size_t i = 0, j = 0; i < m->module->numFunctions; ++i)
    {
        IM3Function f = &m->module->functions[i];
        if (f->import.moduleUtf8 && f->import.fieldUtf8)
        {
            JSValue item = JS_NewObject(ctx);
            JS_DefinePropertyValueStr(ctx, item, "kind", JS_NewString(ctx, "function"), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, item, "module", JS_NewString(ctx, f->import.moduleUtf8), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, item, "name", JS_NewString(ctx, f->import.fieldUtf8), JS_PROP_C_W_E);
            JS_DefinePropertyValueUint32(ctx, imports, j, item, JS_PROP_C_W_E);
            j++;
        }
    }

    // TODO: other import types.

    return imports;
}

static const JSCFunctionListEntry function_list[] = {
    JS_CFUNC_DEF("wasmNewModule", 1, nx_wasm_new_module),
    JS_CFUNC_DEF("wasmModuleExports", 1, nx_wasm_module_exports),
    JS_CFUNC_DEF("wasmModuleImports", 1, nx_wasm_module_imports),
};

void nx_init_wasm(JSContext *ctx, JSValueConst native_obj)
{
    JSRuntime *rt = JS_GetRuntime(ctx);

    JS_NewClassID(&nx_wasm_module_class_id);
    JSClassDef nx_wasm_module_class = {
        "WebAssembly.Module",
        .finalizer = finalizer_wasm_module,
    };
    JS_NewClass(rt, nx_wasm_module_class_id, &nx_wasm_module_class);

    JS_SetPropertyFunctionList(ctx, native_obj, function_list, countof(function_list));
}
