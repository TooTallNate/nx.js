/**
 * Modified from `txiki.js` by Saúl Ibarra Corretgé <s@saghul.net>
 *  - https://github.com/saghul/txiki.js/blob/master/src/wasm.c
 *  - https://github.com/saghul/txiki.js/blob/master/src/js/polyfills/wasm.js
 */
#include "types.h"
#include "wasm.h"
#include <m3_env.h>

static M3Result nx_wasm_js_error = "JS error was thrown";

// https://webassembly.github.io/spec/js-api/index.html#towebassemblyvalue
// static void nx__wasm_towebassemblyvalue(JSContext *ctx, M3ValueType type, const void *stack) {}

// https://webassembly.github.io/spec/js-api/index.html#tojsvalue
static JSValue nx__wasm_tojsvalue(JSContext *ctx, M3ValueType type, const void *stack)
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
    if (!m)
    {
        return JS_ThrowOutOfMemory(ctx);
    }

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

typedef struct
{
    JSContext *ctx;
    JSValue func;
} nx_wasm_imported_func_t;

m3ApiRawFunction(nx_wasm_imported_func)
{
    IM3Function func = _ctx->function;
    IM3FuncType funcType = func->funcType;
    nx_wasm_imported_func_t *js = _ctx->userdata;

    for (int i = 0; i < funcType->numRets; i++)
    {
    }

    // Map the WASM arguments to JS values
    JSValue args[funcType->numArgs];
    for (int i = 0; i < funcType->numArgs; i++)
    {
        u8 type = funcType->types[funcType->numRets + i];
        args[i] = nx__wasm_tojsvalue(js->ctx, type, _sp);
        _sp++;
    }

    // Invoke the JavaScript user function
    JSValue ret_val = JS_Call(js->ctx, js->func, JS_NULL, funcType->numArgs, args);
    if (JS_IsException(ret_val))
    {
        JS_FreeValue(js->ctx, ret_val);
        return nx_wasm_js_error;
    }

    // TODO: map JS return value back to WASM return value(s)
    // m3ApiMultiValueReturnType(int32_t, one);
    // m3ApiGetArg(int32_t, param);
    //  m3ApiGetArg(int64_t, param)
    //  m3ApiGetArg(float, param)
    // m3ApiMultiValueReturn(one, 1);

    JS_FreeValue(js->ctx, ret_val);
    m3ApiSuccess();
}

static JSValue nx__add_module_imports(JSContext *ctx, nx_wasm_instance_t *instance, const char *module_name, JSValueConst module_imports)
{
    if (!JS_IsObject(module_imports))
        return JS_UNDEFINED;

    JSPropertyEnum *tab;
    uint32_t len;
    int flags = JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY;

    // Get the properties of the object
    if (JS_GetOwnPropertyNames(ctx, &tab, &len, module_imports, flags) != 0)
    {
        // TODO: Handle the error
        return JS_EXCEPTION;
    }

    // Iterate over the properties
    for (uint32_t i = 0; i < len; i++)
    {
        JSValue key = JS_AtomToValue(ctx, tab[i].atom);
        if (JS_IsException(key))
        {
            // Handle the error
            return key;
        }

        const char *key_str = JS_ToCString(ctx, key);
        if (key_str)
        {
            // printf("Module: %s, Name: %s\n", module_name, key_str);
            JSValue v = JS_GetPropertyStr(ctx, module_imports, key_str);
            if (JS_IsFunction(ctx, v))
            {
                // TODO: add reference to user function using `Ex`
                nx_wasm_imported_func_t *js = js_malloc(ctx, sizeof(nx_wasm_imported_func_t));
                if (!js)
                {
                    JS_FreeCString(ctx, key_str);
                    JS_FreeValue(ctx, v);
                    return JS_ThrowOutOfMemory(ctx);
                }
                js->ctx = ctx;

                // TODO: when do we de-dup this func? probably when the instance is being finalized?
                js->func = JS_DupValue(ctx, v);

                M3Result r = m3_LinkRawFunctionEx(
                    instance->module,
                    module_name,
                    key_str,
                    NULL,
                    nx_wasm_imported_func,
                    js);
                if (r)
                {
                    JS_FreeCString(ctx, key_str);
                    JS_FreeValue(ctx, v);
                    return nx_throw_wasm_error(ctx, "LinkError", r);
                }
            }
            else
            {
                // TODO: handle other import types
            }

            JS_FreeCString(ctx, key_str);
            JS_FreeValue(ctx, v);
        }
        JS_FreeValue(ctx, key);
        JS_FreeAtom(ctx, tab[i].atom);
    }

    js_free(ctx, tab);
    return JS_UNDEFINED;
}

static JSValue nx_wasm_new_instance(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);

    JSValue obj = JS_NewObjectClass(ctx, nx_wasm_instance_class_id);
    nx_wasm_instance_t *instance = js_mallocz(ctx, sizeof(nx_wasm_instance_t));
    if (!instance)
    {
        return JS_ThrowOutOfMemory(ctx);
    }

    JS_SetOpaque(obj, instance);

    nx_wasm_module_t *m = nx_wasm_module_get(ctx, argv[0]);

    M3Result r = m3_ParseModule(nx_ctx->wasm_env, &instance->module, m->data, m->size);
    // CHECK_NULL(r);  // Should never fail because we already parsed it. TODO: clone it?

    /* Create a runtime per module to avoid symbol clash. */
    instance->runtime = m3_NewRuntime(nx_ctx->wasm_env, /* TODO: adjust */ 512 * 1024, NULL);
    if (!instance->runtime)
    {
        JS_FreeValue(ctx, obj);
        return JS_ThrowOutOfMemory(ctx);
    }

    r = m3_LoadModule(instance->runtime, instance->module);
    if (r)
    {
        JS_FreeValue(ctx, obj);
        return nx_throw_wasm_error(ctx, "LinkError", r);
    }

    // Add the provided imports into the runtime
    JSValue imports = argv[1];
    if (JS_IsObject(imports))
    {
        JSPropertyEnum *tab;
        uint32_t len;
        int flags = JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY;

        // Get the properties of the object
        if (JS_GetOwnPropertyNames(ctx, &tab, &len, imports, flags) != 0)
        {
            // TODO: Handle the error
            return JS_EXCEPTION;
        }

        // Iterate over the properties
        for (uint32_t i = 0; i < len; i++)
        {
            JSValue key = JS_AtomToValue(ctx, tab[i].atom);
            if (JS_IsException(key))
            {
                // Handle the error
                return key;
            }

            const char *key_str = JS_ToCString(ctx, key);
            if (key_str)
            {
                JSValue result = nx__add_module_imports(ctx, instance, key_str, JS_GetPropertyStr(ctx, imports, key_str));
                JS_FreeCString(ctx, key_str);
                if (JS_IsException(result))
                {
                    return result;
                }
                JS_FreeValue(ctx, result);
            }
            JS_FreeValue(ctx, key);
            JS_FreeAtom(ctx, tab[i].atom);
        }

        js_free(ctx, tab);
    }

    instance->loaded = true;

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
    {
        if (r == nx_wasm_js_error)
        {
            // If a JavaScript error was returned then that means an
            // imported function threw an error, so re-throw here
            return JS_EXCEPTION;
        }
        else
        {
            return nx_throw_wasm_error(ctx, "RuntimeError", r);
        }
    }

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
        return nx__wasm_tojsvalue(ctx, m3_GetRetType(func, 0), valptrs[0]);
    }
    else
    {
        JSValue rets = JS_NewArray(ctx);
        for (int i = 0; i < ret_count; i++)
        {
            JS_SetPropertyUint32(ctx, rets, i, nx__wasm_tojsvalue(ctx, m3_GetRetType(func, i), valptrs[i]));
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
