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

static JSClassID nx_wasm_global_class_id;

typedef struct
{
    IM3Global global;
} nx_wasm_global_t;

static nx_wasm_global_t *nx_wasm_global_get(JSContext *ctx, JSValueConst obj)
{
    return JS_GetOpaque2(ctx, obj, nx_wasm_global_class_id);
}

static void finalizer_wasm_global(JSRuntime *rt, JSValue val)
{
    nx_wasm_global_t *g = JS_GetOpaque(val, nx_wasm_global_class_id);
    if (g)
    {
        // Don't need to free `global` since the Runtime instance owns it
        g->global = NULL;
        js_free_rt(rt, g);
    }
}

static JSValue nx_wasm_new_global(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSValue obj = JS_NewObjectClass(ctx, nx_wasm_global_class_id);
    nx_wasm_global_t *g = js_mallocz(ctx, sizeof(nx_wasm_global_t));
    if (!g)
    {
        return JS_ThrowOutOfMemory(ctx);
    }

    JS_SetOpaque(obj, g);

    // Gets defined during import / export instantiation
    g->global = NULL;

    return obj;
}

static JSValue nx_wasm_global_value_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_wasm_global_t *g = nx_wasm_global_get(ctx, argv[0]);
    if (!g)
        return JS_EXCEPTION;

    IM3Global global = g->global;
    if (!global)
    {
        // Not bound
        // TODO: throw error
        return JS_ThrowTypeError(ctx, "Global not defined");
    }

    M3TaggedValue val;
    M3Result r = m3_GetGlobal(global, &val);
    if (r)
    {
        return nx_throw_wasm_error(ctx, "LinkError", r);
    }

    return JS_NewInt32(ctx, val.value.i32);
}

static JSValue nx_wasm_global_value_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_wasm_global_t *g = nx_wasm_global_get(ctx, argv[0]);
    if (!g)
        return JS_EXCEPTION;

    IM3Global global = g->global;
    if (!global)
    {
        // Not bound
        // TODO: throw error
        return JS_ThrowTypeError(ctx, "Global not defined");
    }

    // M3TaggedValue val;
    // val.type = global->type;
    // val.value.i32 = 123;

    JSValue new_val = argv[1];
    switch (global->type)
    {
    case c_m3Type_i32:
    {
        if (JS_ToInt32(ctx, &global->i32Value, new_val))
        {
            // TODO: handle error
        }
        break;
    };
    case c_m3Type_i64:
    {
        if (JS_ToInt64(ctx, &global->i64Value, new_val))
        {
            // TODO: handle error
        }
        break;
    };
    case c_m3Type_f32:
    case c_m3Type_f64:
    {
        if (JS_ToFloat64(ctx, &global->f64Value, new_val))
        {
            // TODO: handle error
        }
        break;
    };
    }

    // M3Result r = m3_SetGlobal(global, &val);
    // if (r)
    //{
    //     return nx_throw_wasm_error(ctx, "LinkError", r);
    // }

    return JS_UNDEFINED;
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

static JSValue find_matching_import(JSContext *ctx, M3ImportInfo *info, JSValue imports_array, size_t imports_array_length)
{
    for (size_t i = 0; i < imports_array_length; i++)
    {
        JSValue entry = JS_GetPropertyUint32(ctx, imports_array, i);

        const char *module_name = JS_ToCString(ctx, JS_GetPropertyStr(ctx, entry, "module"));
        if (strcmp(info->moduleUtf8, module_name) != 0)
        {
            JS_FreeCString(ctx, module_name);
            continue;
        }

        const char *field_name = JS_ToCString(ctx, JS_GetPropertyStr(ctx, entry, "name"));
        if (strcmp(info->fieldUtf8, field_name) != 0)
        {
            JS_FreeCString(ctx, module_name);
            JS_FreeCString(ctx, field_name);
            continue;
        }

        JS_FreeCString(ctx, module_name);
        JS_FreeCString(ctx, field_name);
        return entry;
    }

    return JS_UNDEFINED;
}

static JSValue nx_wasm_new_instance(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);

    JSValue opaque = JS_NewObjectClass(ctx, nx_wasm_instance_class_id);
    nx_wasm_instance_t *instance = js_mallocz(ctx, sizeof(nx_wasm_instance_t));
    if (!instance)
    {
        return JS_ThrowOutOfMemory(ctx);
    }

    JS_SetOpaque(opaque, instance);

    nx_wasm_module_t *m = nx_wasm_module_get(ctx, argv[0]);

    M3Result r = m3_ParseModule(nx_ctx->wasm_env, &instance->module, m->data, m->size);
    // CHECK_NULL(r);  // Should never fail because we already parsed it. TODO: clone it?

    /* Create a runtime per module to avoid symbol clash. */
    instance->runtime = m3_NewRuntime(nx_ctx->wasm_env, /* TODO: adjust */ 512 * 1024, NULL);
    if (!instance->runtime)
    {
        JS_FreeValue(ctx, opaque);
        return JS_ThrowOutOfMemory(ctx);
    }

    // TODO: handle imported memory here?

    r = m3_LoadModule(instance->runtime, instance->module);
    if (r)
    {
        JS_FreeValue(ctx, opaque);
        return nx_throw_wasm_error(ctx, "LinkError", r);
    }

    // Process the provided "imports" into the runtime,
    // instantiate the defined "exports" from the runtime
    JSValue imports_array = argv[1];
    JSValue exports_array = JS_NewArray(ctx);
    size_t exports_index = 0;

    uint32_t imports_array_length;
    if (JS_ToUint32(ctx, &imports_array_length, JS_GetPropertyStr(ctx, imports_array, "length")))
    {
        return JS_EXCEPTION;
    }

    for (size_t i = 0; i < instance->module->numFunctions; ++i)
    {
        IM3Function f = &instance->module->functions[i];
        if (f->import.moduleUtf8 && f->import.fieldUtf8)
        {
            // Imported `Function`
            JSValue matching_import = find_matching_import(ctx, &f->import, imports_array, imports_array_length);
            if (JS_IsUndefined(matching_import))
            {
                JS_ThrowTypeError(ctx, "Missing import function \"%s.%s\"", f->import.moduleUtf8, f->import.fieldUtf8);
            }

            // TODO: validate "kind === 'function'"

            JSValue v = JS_GetPropertyStr(ctx, matching_import, "val");
            if (JS_IsFunction(ctx, v))
            {
                nx_wasm_imported_func_t *js = js_malloc(ctx, sizeof(nx_wasm_imported_func_t));
                if (!js)
                {
                    JS_FreeValue(ctx, v);
                    return JS_ThrowOutOfMemory(ctx);
                }
                js->ctx = ctx;

                // TODO: when do we de-dup this func? probably when the instance is being finalized?
                js->func = JS_DupValue(ctx, v);

                M3Result r = m3_LinkRawFunctionEx(
                    instance->module,
                    f->import.moduleUtf8,
                    f->import.fieldUtf8,
                    NULL,
                    nx_wasm_imported_func,
                    js);
                if (r)
                {
                    JS_FreeValue(ctx, v);
                    return nx_throw_wasm_error(ctx, "LinkError", r);
                }
            }
        }
        else
        {
            // Exported `Function`
            JSValue item = JS_NewObject(ctx);
            JS_DefinePropertyValueStr(ctx, item, "kind", JS_NewString(ctx, "function"), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, item, "name", JS_NewString(ctx, f->export_name), JS_PROP_C_W_E);
            JS_DefinePropertyValueUint32(ctx, exports_array, exports_index++, item, JS_PROP_C_W_E);
            // TODO: we can optimize exports by passing the function reference back to
            // JS now, so that we don't need to call `FindFunction()` during every invoke
        }
    }

    for (size_t i = 0; i < instance->module->numGlobals; i++)
    {
        const IM3Global g = &instance->module->globals[i];
        if (g->imported && g->import.moduleUtf8 && g->import.fieldUtf8)
        {
            // Imported `Global`

            JSValue matching_import = find_matching_import(ctx, &g->import, imports_array, imports_array_length);
            if (JS_IsUndefined(matching_import))
            {
                JS_ThrowTypeError(ctx, "Missing import global \"%s.%s\"", g->import.moduleUtf8, g->import.fieldUtf8);
            }

            // TODO: validate "kind === 'global'"

            JSValue v = JS_GetPropertyStr(ctx, matching_import, "val");

            nx_wasm_global_t *nx_g = nx_wasm_global_get(ctx, v);
            nx_g->global = g;

            JSValue initial_value = JS_GetPropertyStr(ctx, matching_import, "i");
            switch (g->type)
            {
            case c_m3Type_i32:
            {
                if (JS_ToInt32(ctx, &g->i32Value, initial_value))
                {
                    // TODO: handle error
                }
                break;
            };
            case c_m3Type_i64:
            {
                if (JS_ToInt64(ctx, &g->i64Value, initial_value))
                {
                    // TODO: handle error
                }
                break;
            };
            case c_m3Type_f32:
            case c_m3Type_f64:
            {
                if (JS_ToFloat64(ctx, &g->f64Value, initial_value))
                {
                    // TODO: handle error
                }
                break;
            };
            }
        }
        else
        {
            // Exported `Global`
            JSValue op = nx_wasm_new_global(ctx, JS_UNDEFINED, 0, NULL);
            if (JS_IsException(op))
            {
                return JS_EXCEPTION;
            }
            nx_wasm_global_t *nx_g = nx_wasm_global_get(ctx, op);
            if (!nx_g)
            {
                return JS_EXCEPTION;
            }
            nx_g->global = g;

            JSValue item = JS_NewObject(ctx);
            JS_DefinePropertyValueStr(ctx, item, "kind", JS_NewString(ctx, "global"), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, item, "name", JS_NewString(ctx, g->name), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, item, "val", op, JS_PROP_C_W_E);
            // TODO: value ("i32")
            // TODO: mutable (true, false)
            JS_DefinePropertyValueUint32(ctx, exports_array, exports_index++, item, JS_PROP_C_W_E);
        }
    }

    // Find the memory export
    uint32_t memorySize;
    // wasm3 currently only supports one memory region.
    // `i_memoryIndex` must be zero.
    uint32_t i_memoryIndex = 0;
    uint8_t *memory = m3_GetMemory(instance->runtime, &memorySize, i_memoryIndex);

    if (memory && !instance->module->memoryImported)
    {
        // Exported `Memory`
        JSValue buf = JS_NewArrayBuffer(ctx, memory, memorySize, NULL, NULL, false);
        if (JS_IsException(buf))
        {
            return buf;
        }

        // TODO: Seems like the exported name is not saved within wasm3 - assume "memory" for now
        const char *name = "memory";

        JSValue item = JS_NewObject(ctx);
        JS_DefinePropertyValueStr(ctx, item, "kind", JS_NewString(ctx, "memory"), JS_PROP_C_W_E);
        JS_DefinePropertyValueStr(ctx, item, "name", JS_NewString(ctx, name), JS_PROP_C_W_E);
        JS_DefinePropertyValueStr(ctx, item, "val", buf, JS_PROP_C_W_E);
        // TODO: maximum / shared?
        JS_DefinePropertyValueUint32(ctx, exports_array, exports_index++, item, JS_PROP_C_W_E);
    }

    instance->loaded = true;

    JSValue rtn = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, rtn, 0, opaque);
    JS_SetPropertyUint32(ctx, rtn, 1, exports_array);
    return rtn;
}

static JSValue nx_wasm_module_imports(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_wasm_module_t *m = nx_wasm_module_get(ctx, argv[0]);
    if (!m)
        return JS_EXCEPTION;

    JSValue imports = JS_NewArray(ctx);
    if (JS_IsException(imports))
        return imports;

    size_t index = 0;
    for (size_t i = 0; i < m->module->numFunctions; ++i)
    {
        IM3Function f = &m->module->functions[i];
        if (f->import.moduleUtf8 && f->import.fieldUtf8)
        {
            JSValue item = JS_NewObject(ctx);
            JS_DefinePropertyValueStr(ctx, item, "kind", JS_NewString(ctx, "function"), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, item, "module", JS_NewString(ctx, f->import.moduleUtf8), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, item, "name", JS_NewString(ctx, f->import.fieldUtf8), JS_PROP_C_W_E);
            JS_DefinePropertyValueUint32(ctx, imports, index++, item, JS_PROP_C_W_E);
        }
    }

    for (size_t i = 0; i < m->module->numGlobals; i++)
    {
        IM3Global g = &m->module->globals[i];
        if (g->imported && g->import.moduleUtf8 && g->import.fieldUtf8)
        {
            JSValue item = JS_NewObject(ctx);
            JS_DefinePropertyValueStr(ctx, item, "kind", JS_NewString(ctx, "global"), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, item, "module", JS_NewString(ctx, g->import.moduleUtf8), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, item, "name", JS_NewString(ctx, g->import.fieldUtf8), JS_PROP_C_W_E);
            JS_DefinePropertyValueUint32(ctx, imports, index++, item, JS_PROP_C_W_E);
        }
    }

    // if (m->module->memoryImported) {
    //     JSValue item = JS_NewObject(ctx);
    //     JS_DefinePropertyValueStr(ctx, item, "kind", JS_NewString(ctx, "memory"), JS_PROP_C_W_E);
    //     JS_DefinePropertyValueStr(ctx, item, "module", JS_NewString(ctx, "?"), JS_PROP_C_W_E);
    //     JS_DefinePropertyValueStr(ctx, item, "name", JS_NewString(ctx, "?"), JS_PROP_C_W_E);
    //     JS_DefinePropertyValueUint32(ctx, imports, index++, item, JS_PROP_C_W_E);
    // }

    // TODO: "table" import types (seems that perhaps wasm3 doesn't currently support?)

    return imports;
}

static JSValue nx_wasm_module_exports(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_wasm_module_t *m = nx_wasm_module_get(ctx, argv[0]);
    if (!m)
        return JS_EXCEPTION;

    JSValue exports = JS_NewArray(ctx);
    if (JS_IsException(exports))
        return exports;

    size_t index = 0;
    for (size_t i = 0; i < m->module->numFunctions; ++i)
    {
        IM3Function f = &m->module->functions[i];
        if (f->export_name)
        {
            JSValue item = JS_NewObject(ctx);
            JS_DefinePropertyValueStr(ctx, item, "kind", JS_NewString(ctx, "function"), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, item, "name", JS_NewString(ctx, f->export_name), JS_PROP_C_W_E);
            JS_DefinePropertyValueUint32(ctx, exports, index++, item, JS_PROP_C_W_E);
        }
    }

    for (size_t i = 0; i < m->module->numGlobals; ++i)
    {
        IM3Global g = &m->module->globals[i];
        if (!g->imported)
        {
            JSValue item = JS_NewObject(ctx);
            JS_DefinePropertyValueStr(ctx, item, "kind", JS_NewString(ctx, "global"), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, item, "name", JS_NewString(ctx, g->name), JS_PROP_C_W_E);
            JS_DefinePropertyValueUint32(ctx, exports, index++, item, JS_PROP_C_W_E);
        }
    }

    // TODO: other export types.

    return exports;
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
    JS_CFUNC_DEF("wasmNewGlobal", 1, nx_wasm_new_global),
    JS_CFUNC_DEF("wasmModuleExports", 1, nx_wasm_module_exports),
    JS_CFUNC_DEF("wasmModuleImports", 1, nx_wasm_module_imports),
    JS_CFUNC_DEF("wasmGlobalGet", 1, nx_wasm_global_value_get),
    JS_CFUNC_DEF("wasmGlobalSet", 1, nx_wasm_global_value_set),
    JS_CFUNC_DEF("wasmCallFunc", 1, nx_wasm_call_func),
};

void nx_init_wasm(JSContext *ctx, JSValueConst native_obj)
{
    JSRuntime *rt = JS_GetRuntime(ctx);

    /* WebAssembly.Global */
    JS_NewClassID(&nx_wasm_global_class_id);
    JSClassDef nx_wasm_global_class = {
        "WebAssembly.Global",
        .finalizer = finalizer_wasm_global,
    };
    JS_NewClass(rt, nx_wasm_global_class_id, &nx_wasm_global_class);

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
