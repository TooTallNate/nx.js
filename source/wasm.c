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
    uint8_t *data;
    size_t size;
} nx_wasm_module_t;

static nx_wasm_module_t *nx_wasm_module_get(JSContext *ctx, JSValueConst obj)
{
    return JS_GetOpaque2(ctx, obj, nx_wasm_module_class_id);
}

static void finalizer_wasm_module(JSRuntime *rt, JSValue val)
{
    nx_wasm_module_t *m = JS_GetOpaque(val, nx_wasm_module_class_id);
    if (m)
    {
        if (m->module)
            m3_FreeModule(m->module);
        js_free_rt(rt, m);
    }
}

static JSClassID nx_wasm_instance_class_id;

typedef struct
{
    IM3Runtime runtime;
    IM3Module module;
    bool loaded;
} nx_wasm_instance_t;

static nx_wasm_instance_t *nx_wasm_instance_get(JSContext *ctx, JSValueConst obj)
{
    return JS_GetOpaque2(ctx, obj, nx_wasm_instance_class_id);
}

static void finalizer_wasm_instance(JSRuntime *rt, JSValue val)
{
    nx_wasm_instance_t *i = JS_GetOpaque(val, nx_wasm_instance_class_id);
    if (i)
    {
        if (i->module)
        {
            // Free the module, only if it wasn't previously loaded.
            if (!i->loaded)
                m3_FreeModule(i->module);
        }
        if (i->runtime)
            m3_FreeRuntime(i->runtime);
        js_free_rt(rt, i);
    }
}

static JSValue nx_wasm_new_module(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);

    if (nx_ctx->wasm_env == NULL)
    {
        nx_ctx->wasm_env = m3_NewEnvironment();
    }

    JSValue obj = JS_NewObjectClass(ctx, nx_wasm_module_class_id);
    nx_wasm_module_t *m = js_mallocz(ctx, sizeof(nx_wasm_module_t));
    // TODO: OOM error handling

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

    m->data = buf;
    m->size = size;

    return obj;
}

static JSValue nx_wasm_new_instance(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);

    JSValue obj = JS_NewObjectClass(ctx, nx_wasm_instance_class_id);
    nx_wasm_instance_t *i = js_mallocz(ctx, sizeof(nx_wasm_instance_t));
    // TODO: OOM error handling

    JS_SetOpaque(obj, i);

    nx_wasm_module_t *m = nx_wasm_module_get(ctx, argv[0]);

    M3Result r = m3_ParseModule(nx_ctx->wasm_env, &i->module, m->data, m->size);
    // CHECK_NULL(r);  // Should never fail because we already parsed it. TODO: clone it?

    /* Create a runtime per module to avoid symbol clash. */
    i->runtime = m3_NewRuntime(nx_ctx->wasm_env, /* TODO: adjust */ 512 * 1024, NULL);
    if (!i->runtime)
    {
        JS_FreeValue(ctx, obj);
        return JS_ThrowOutOfMemory(ctx);
    }

    // TODO: add imports

    r = m3_LoadModule(i->runtime, i->module);
    if (r)
    {
        JS_FreeValue(ctx, obj);
        return nx_throw_wasm_error(ctx, "LinkError", r);
    }

    i->loaded = true;

    return obj;
}

static JSValue nx_wasm_module_exports(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_wasm_module_t *m = nx_wasm_module_get(ctx, argv[0]);
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
    nx_wasm_module_t *m = nx_wasm_module_get(ctx, argv[0]);
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

static JSValue nx__wasm_result(JSContext *ctx, M3ValueType type, const void *stack)
{
    switch (type)
    {
    case c_m3Type_i32:
    {
        int32_t val = *(int32_t *)stack;
        return JS_NewInt32(ctx, val);
    }
    case c_m3Type_i64:
    {
        int64_t val = *(int64_t *)stack;
        if (val == (int32_t)val)
            return JS_NewInt32(ctx, (int32_t)val);
        else
            return JS_NewBigInt64(ctx, val);
    }
    case c_m3Type_f32:
    {
        float val = *(float *)stack;
        return JS_NewFloat64(ctx, (double)val);
    }
    case c_m3Type_f64:
    {
        double val = *(double *)stack;
        return JS_NewFloat64(ctx, val);
    }
    default:
        return JS_UNDEFINED;
    }
}

static JSValue nx_wasm_call_func(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_wasm_instance_t *i = nx_wasm_instance_get(ctx, argv[0]);
    if (!i)
        return JS_EXCEPTION;

    const char *fname = JS_ToCString(ctx, argv[1]);
    if (!fname)
        return JS_EXCEPTION;

    IM3Function func;
    M3Result r = m3_FindFunction(&func, i->runtime, fname);
    if (r)
    {
        JS_FreeCString(ctx, fname);
        return nx_throw_wasm_error(ctx, "RuntimeError", r);
    }

    JS_FreeCString(ctx, fname);

    int nargs = argc - 2;
    if (nargs == 0)
    {
        r = m3_Call(func, 0, NULL);
    }
    else
    {
        const char *m3_argv[nargs + 1];
        for (int i = 0; i < nargs; i++)
        {
            m3_argv[i] = JS_ToCString(ctx, argv[i + 2]);
        }
        m3_argv[nargs] = NULL;
        r = m3_CallArgv(func, nargs, m3_argv);
        for (int i = 0; i < nargs; i++)
        {
            JS_FreeCString(ctx, m3_argv[i]);
        }
    }

    if (r)
        return nx_throw_wasm_error(ctx, "RuntimeError", r);

    // https://webassembly.org/docs/js/ See "ToJSValue"
    // NOTE: here we support returning BigInt, because we can.

    int ret_count = m3_GetRetCount(func);

    if (ret_count == 0)
    {
        return JS_UNDEFINED;
    }

    uint64_t valbuff[ret_count];
    const void *valptrs[ret_count];
    memset(valbuff, 0, sizeof(valbuff));
    for (int i = 0; i < ret_count; i++)
    {
        valptrs[i] = &valbuff[i];
    }

    r = m3_GetResults(func, ret_count, valptrs);
    if (r)
        return nx_throw_wasm_error(ctx, "RuntimeError", r);

    if (ret_count == 1)
    {
        return nx__wasm_result(ctx, m3_GetRetType(func, 0), valptrs[0]);
    }
    else
    {
        JSValue rets = JS_NewArray(ctx);
        for (int i = 0; i < ret_count; i++)
        {
            JS_SetPropertyUint32(ctx, rets, i, nx__wasm_result(ctx, m3_GetRetType(func, i), valptrs[i]));
        }
        return rets;
    }
}

static const JSCFunctionListEntry function_list[] = {
    JS_CFUNC_DEF("wasmNewModule", 1, nx_wasm_new_module),
    JS_CFUNC_DEF("wasmNewInstance", 1, nx_wasm_new_instance),
    JS_CFUNC_DEF("wasmModuleExports", 1, nx_wasm_module_exports),
    JS_CFUNC_DEF("wasmModuleImports", 1, nx_wasm_module_imports),
    JS_CFUNC_DEF("wasmCallFunc", 1, nx_wasm_call_func),
};

void nx_init_wasm(JSContext *ctx, JSValueConst native_obj)
{
    JSRuntime *rt = JS_GetRuntime(ctx);

    /* WebAssembly.Module */
    JS_NewClassID(&nx_wasm_module_class_id);
    JSClassDef nx_wasm_module_class = {
        "WebAssembly.Module",
        .finalizer = finalizer_wasm_module,
    };
    JS_NewClass(rt, nx_wasm_module_class_id, &nx_wasm_module_class);

    /* WebAssembly.Instance */
    JS_NewClassID(&nx_wasm_instance_class_id);
    JSClassDef nx_wasm_instance_class = {
        "WebAssembly.Instance",
        .finalizer = finalizer_wasm_instance,
    };
    JS_NewClass(rt, nx_wasm_instance_class_id, &nx_wasm_instance_class);

    JS_SetPropertyFunctionList(ctx, native_obj, function_list, countof(function_list));
}
