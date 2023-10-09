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
static int nx__wasm_towebassemblyvalue(JSContext *ctx, JSValueConst val, M3ValueType type, void *stack)
{
    int r = 0;
    switch (type)
    {
    case c_m3Type_i32:
    {
        r = JS_ToInt32(ctx, (int32_t *)stack, val);
        break;
    };
    case c_m3Type_i64:
    {
        r = JS_ToInt64(ctx, (int64_t *)stack, val);
        break;
    };
    case c_m3Type_f32:
    case c_m3Type_f64:
    {
        r = JS_ToFloat64(ctx, (double *)stack, val);
        break;
    };
    case c_m3Type_none:
    case c_m3Type_unknown:
    {
        /* shrug */
        break;
    }
    }
    return r;
}

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

static JSClassID nx_wasm_memory_class_id;

typedef struct
{
    IM3Memory mem;
    bool needs_free;
    int is_shared;
} nx_wasm_memory_t;

static nx_wasm_memory_t *nx_wasm_memory_get(JSContext *ctx, JSValueConst obj)
{
    return JS_GetOpaque2(ctx, obj, nx_wasm_memory_class_id);
}

static void finalizer_wasm_memory(JSRuntime *rt, JSValue val)
{
    nx_wasm_memory_t *data = JS_GetOpaque(val, nx_wasm_memory_class_id);
    if (data)
    {
        if (data->needs_free && data->mem)
        {
            if (data->mem->mallocated)
            {
                m3_Free(data->mem->mallocated);
            }
            js_free_rt(rt, data->mem);
        }
        js_free_rt(rt, data);
    }
}

static JSValue nx_wasm_memory_new_(JSContext *ctx)
{
    JSValue obj = JS_NewObjectClass(ctx, nx_wasm_memory_class_id);
    nx_wasm_memory_t *data = js_mallocz(ctx, sizeof(nx_wasm_memory_t));
    if (!data)
    {
        JS_ThrowOutOfMemory(ctx);
        return JS_EXCEPTION;
    }
    JS_SetOpaque(obj, data);
    return obj;
}

static JSClassID nx_wasm_table_class_id;

typedef struct
{
    IM3Function *table;
    u32 *table_size;
} nx_wasm_table_t;

static nx_wasm_table_t *nx_wasm_table_get(JSContext *ctx, JSValueConst obj)
{
    return JS_GetOpaque2(ctx, obj, nx_wasm_table_class_id);
}

static void finalizer_wasm_table(JSRuntime *rt, JSValue val)
{
    nx_wasm_table_t *data = JS_GetOpaque(val, nx_wasm_table_class_id);
    if (data)
    {
        js_free_rt(rt, data);
    }
}

static JSValue nx_wasm_table_new_(JSContext *ctx)
{
    JSValue obj = JS_NewObjectClass(ctx, nx_wasm_table_class_id);
    nx_wasm_table_t *data = js_mallocz(ctx, sizeof(nx_wasm_table_t));
    if (!data)
    {
        JS_ThrowOutOfMemory(ctx);
        return JS_EXCEPTION;
    }
    JS_SetOpaque(obj, data);
    return obj;
}

static JSClassID nx_wasm_exported_func_class_id;

typedef struct
{
    IM3Function function;
} nx_wasm_exported_func_t;

static nx_wasm_exported_func_t *nx_wasm_exported_func_get(JSContext *ctx, JSValueConst obj)
{
    return JS_GetOpaque2(ctx, obj, nx_wasm_exported_func_class_id);
}

static void finalizer_wasm_exported_func(JSRuntime *rt, JSValue val)
{
    nx_wasm_exported_func_t *data = JS_GetOpaque(val, nx_wasm_exported_func_class_id);
    if (data)
    {
        js_free_rt(rt, data);
    }
}

static JSValue nx_wasm_exported_func_new(JSContext *ctx, IM3Function func)
{
    JSValue obj = JS_NewObjectClass(ctx, nx_wasm_exported_func_class_id);
    nx_wasm_exported_func_t *data = js_mallocz(ctx, sizeof(nx_wasm_exported_func_t));
    if (!data)
    {
        JS_ThrowOutOfMemory(ctx);
        return JS_EXCEPTION;
    }
    data->function = func;
    JS_SetOpaque(obj, data);
    return obj;
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
        JS_ThrowOutOfMemory(ctx);
        return JS_EXCEPTION;
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
        return JS_ThrowTypeError(ctx, "Global not defined");
    }

    M3TaggedValue val;
    M3Result r = m3_GetGlobal(global, &val);
    if (r)
    {
        return nx_throw_wasm_error(ctx, "LinkError", r);
    }

    return nx__wasm_tojsvalue(ctx, val.type, &val.value);
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
        return JS_ThrowTypeError(ctx, "Global not defined");
    }

    if (nx__wasm_towebassemblyvalue(ctx, argv[1], global->type, &global->i32Value))
        return JS_EXCEPTION;

    return JS_UNDEFINED;
}

static JSClassID nx_wasm_instance_class_id;

typedef struct
{
    IM3Runtime runtime;
    IM3Module module;
    bool loaded;
} nx_wasm_instance_t;

// static nx_wasm_instance_t *nx_wasm_instance_get(JSContext *ctx, JSValueConst obj)
//{
//     return JS_GetOpaque2(ctx, obj, nx_wasm_instance_class_id);
// }

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
        JS_ThrowOutOfMemory(ctx);
        return JS_EXCEPTION;
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

    uint64_t *retValAddr = _sp;
    _sp += funcType->numRets;

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

    // Map the JS return value to WASM
    if (funcType->numRets > 0)
    {
        if (nx__wasm_towebassemblyvalue(js->ctx, ret_val, funcType->types[0], retValAddr))
        {
            JS_FreeValue(js->ctx, ret_val);
            return nx_wasm_js_error;
        }
        // TODO: handle multi-return values when JS returns an Array?
    }

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
            JS_FreeValue(ctx, entry);
            continue;
        }

        const char *field_name = JS_ToCString(ctx, JS_GetPropertyStr(ctx, entry, "name"));
        if (strcmp(info->fieldUtf8, field_name) != 0)
        {
            JS_FreeCString(ctx, module_name);
            JS_FreeCString(ctx, field_name);
            JS_FreeValue(ctx, entry);
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
        JS_ThrowOutOfMemory(ctx);
        return JS_EXCEPTION;
    }

    JS_SetOpaque(opaque, instance);

    nx_wasm_module_t *m = nx_wasm_module_get(ctx, argv[0]);

    M3Result r = m3_ParseModule(nx_ctx->wasm_env, &instance->module, m->data, m->size);
    // CHECK_NULL(r);  // Should never fail because we already parsed it. TODO: clone it?

    /* Create a runtime per module to avoid symbol clash. */
    IM3Runtime runtime = m3_NewRuntime(nx_ctx->wasm_env, /* TODO: adjust */ 512 * 1024, NULL);
    if (!runtime)
    {
        JS_FreeValue(ctx, opaque);
        JS_ThrowOutOfMemory(ctx);
        return JS_EXCEPTION;
    }
    instance->runtime = runtime;

    JSValue imports_array = argv[1];
    uint32_t imports_array_length;
    if (JS_ToUint32(ctx, &imports_array_length, JS_GetPropertyStr(ctx, imports_array, "length")))
    {
        return JS_EXCEPTION;
    }

    /* When the WASM module declares the memory as an import, we need to "map"
       the provided `WebAssembly.Memory` data into the runtime here, before
       loading the module. */
    if (instance->module->memoryImported)
    {
        M3ImportInfo *import = &instance->module->memoryImport;
        JSValue matching_import = find_matching_import(ctx, import, imports_array, imports_array_length);
        if (JS_IsUndefined(matching_import))
        {
            JS_ThrowTypeError(ctx, "Missing import memory \"%s.%s\"", import->moduleUtf8, import->fieldUtf8);
            return JS_EXCEPTION;
        }

        JSValue v = JS_GetPropertyStr(ctx, matching_import, "val");
        nx_wasm_memory_t *data = nx_wasm_memory_get(ctx, v);

        memcpy(&runtime->memory, data->mem, sizeof(M3Memory));
        runtime->memory.mallocated->runtime = runtime;
        runtime->memory.mallocated->maxStack = (m3slot_t *)runtime->stack + runtime->numStackSlots;

        // TODO: what if "numPages" or "maxPages" conflict?

        if (data->needs_free)
        {
            js_free(ctx, data->mem);
        }
        data->mem = &runtime->memory;
        data->needs_free = false;

        JS_FreeValue(ctx, v);
        JS_FreeValue(ctx, matching_import);
    }

    r = m3_LoadModule(runtime, instance->module);
    if (r)
    {
        JS_FreeValue(ctx, opaque);
        return nx_throw_wasm_error(ctx, "LinkError", r);
    }

    // Process the provided "imports" into the runtime,
    // instantiate the defined "exports" from the runtime
    JSValue exports_array = JS_NewArray(ctx);
    size_t exports_index = 0;

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
                return JS_EXCEPTION;
            }

            JSValue v = JS_GetPropertyStr(ctx, matching_import, "val");
            if (JS_IsFunction(ctx, v))
            {
                nx_wasm_imported_func_t *js = js_malloc(ctx, sizeof(nx_wasm_imported_func_t));
                if (!js)
                {
                    JS_FreeValue(ctx, v);
                    JS_FreeValue(ctx, matching_import);
                    JS_ThrowOutOfMemory(ctx);
                    return JS_EXCEPTION;
                }
                js->ctx = ctx;

                // TODO: when do we de-dup this func? probably when the instance is being finalized?
                js->func = JS_DupValue(ctx, v);
                // js->func = v;

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
                    JS_FreeValue(ctx, matching_import);
                    JS_FreeValue(ctx, js->func);
                    js_free(ctx, js);
                    return nx_throw_wasm_error(ctx, "LinkError", r);
                }
            }

            JS_FreeValue(ctx, v);
            JS_FreeValue(ctx, matching_import);
        }
        else if (f->export_name)
        {
            // Exported `Function`
            JSValue val = nx_wasm_exported_func_new(ctx, f);
            if (JS_IsException(val))
                return JS_EXCEPTION;

            JSValue item = JS_NewObject(ctx);
            JS_DefinePropertyValueStr(ctx, item, "kind", JS_NewString(ctx, "function"), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, item, "name", JS_NewString(ctx, f->export_name), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, item, "val", val, JS_PROP_C_W_E);
            JS_DefinePropertyValueUint32(ctx, exports_array, exports_index++, item, JS_PROP_C_W_E);
        }
    }

    for (size_t i = 0; i < instance->module->numGlobals; i++)
    {
        const IM3Global g = &instance->module->globals[i];
        if (g->imported)
        {
            // Imported `Global`
            JSValue matching_import = find_matching_import(ctx, &g->import, imports_array, imports_array_length);
            if (JS_IsUndefined(matching_import))
            {
                JS_ThrowTypeError(ctx, "Missing import global \"%s.%s\"", g->import.moduleUtf8, g->import.fieldUtf8);
                return JS_EXCEPTION;
            }

            JSValue v = JS_GetPropertyStr(ctx, matching_import, "val");

            // TODO: handle "val" being a Number

            nx_wasm_global_t *nx_g = nx_wasm_global_get(ctx, v);
            nx_g->global = g;

            JSValue initial_value = JS_GetPropertyStr(ctx, matching_import, "i");
            JS_FreeValue(ctx, v);

            if (nx__wasm_towebassemblyvalue(ctx, initial_value, g->type, &g->i32Value))
            {
                JS_FreeValue(ctx, initial_value);
                return JS_EXCEPTION;
            }

            JS_FreeValue(ctx, initial_value);
        }
        else if (g->name)
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

    if (instance->module->memoryExportName)
    {
        // Exported `Memory`
        JSValue val = nx_wasm_memory_new_(ctx);
        if (JS_IsException(val))
            return JS_EXCEPTION;

        nx_wasm_memory_t *data = nx_wasm_memory_get(ctx, val);
        data->mem = &runtime->memory;
        data->needs_free = false;

        JSValue item = JS_NewObject(ctx);
        JS_DefinePropertyValueStr(ctx, item, "kind", JS_NewString(ctx, "memory"), JS_PROP_C_W_E);
        JS_DefinePropertyValueStr(ctx, item, "name", JS_NewString(ctx, instance->module->memoryExportName), JS_PROP_C_W_E);
        JS_DefinePropertyValueStr(ctx, item, "val", val, JS_PROP_C_W_E);
        JS_DefinePropertyValueUint32(ctx, exports_array, exports_index++, item, JS_PROP_C_W_E);
    }

    if (instance->module->table0ExportName)
    {
        // Exported `Table`
        JSValue val = nx_wasm_table_new_(ctx);
        if (JS_IsException(val))
            return JS_EXCEPTION;

        nx_wasm_table_t *data = nx_wasm_table_get(ctx, val);
        data->table = instance->module->table0;
        data->table_size = &instance->module->table0Size;

        JSValue item = JS_NewObject(ctx);
        JS_DefinePropertyValueStr(ctx, item, "kind", JS_NewString(ctx, "table"), JS_PROP_C_W_E);
        JS_DefinePropertyValueStr(ctx, item, "name", JS_NewString(ctx, instance->module->table0ExportName), JS_PROP_C_W_E);
        JS_DefinePropertyValueStr(ctx, item, "val", val, JS_PROP_C_W_E);
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

    if (m->module->memoryImported)
    {
        JSValue item = JS_NewObject(ctx);
        JS_DefinePropertyValueStr(ctx, item, "kind", JS_NewString(ctx, "memory"), JS_PROP_C_W_E);
        JS_DefinePropertyValueStr(ctx, item, "module", JS_NewString(ctx, m->module->memoryImport.moduleUtf8), JS_PROP_C_W_E);
        JS_DefinePropertyValueStr(ctx, item, "name", JS_NewString(ctx, m->module->memoryImport.fieldUtf8), JS_PROP_C_W_E);
        JS_DefinePropertyValueUint32(ctx, imports, index++, item, JS_PROP_C_W_E);
    }

    // TODO: "table" import types (wasm3 doesn't currently support)

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
        if (!g->imported && g->name)
        {
            JSValue item = JS_NewObject(ctx);
            JS_DefinePropertyValueStr(ctx, item, "kind", JS_NewString(ctx, "global"), JS_PROP_C_W_E);
            JS_DefinePropertyValueStr(ctx, item, "name", JS_NewString(ctx, g->name), JS_PROP_C_W_E);
            JS_DefinePropertyValueUint32(ctx, exports, index++, item, JS_PROP_C_W_E);
        }
    }

    if (!m->module->memoryImported && m->module->memoryExportName)
    {
        JSValue item = JS_NewObject(ctx);
        JS_DefinePropertyValueStr(ctx, item, "kind", JS_NewString(ctx, "memory"), JS_PROP_C_W_E);
        JS_DefinePropertyValueStr(ctx, item, "name", JS_NewString(ctx, m->module->memoryExportName), JS_PROP_C_W_E);
        JS_DefinePropertyValueUint32(ctx, exports, index++, item, JS_PROP_C_W_E);
    }

    if (m->module->table0ExportName)
    {
        JSValue item = JS_NewObject(ctx);
        JS_DefinePropertyValueStr(ctx, item, "kind", JS_NewString(ctx, "table"), JS_PROP_C_W_E);
        JS_DefinePropertyValueStr(ctx, item, "name", JS_NewString(ctx, m->module->table0ExportName), JS_PROP_C_W_E);
        JS_DefinePropertyValueUint32(ctx, exports, index++, item, JS_PROP_C_W_E);
    }

    return exports;
}

static JSValue nx_wasm_call_func(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_wasm_exported_func_t *data = nx_wasm_exported_func_get(ctx, argv[0]);
    if (!data)
        return JS_EXCEPTION;

    IM3Function func = data->function;
    if (!func)
    {
        return nx_throw_wasm_error(ctx, "RuntimeError", "Missing function reference");
    }

    M3Result r = m3Err_none;
    if (!func->compiled)
    {
        r = CompileFunction(func);
    }
    if (r)
    {
        return nx_throw_wasm_error(ctx, "RuntimeError", r);
    }

    int nargs = m3_GetArgCount(func);
    if (nargs == 0)
    {
        r = m3_Call(func, 0, NULL);
    }
    else
    {
        const char *m3_argv[nargs + 1];
        for (int i = 0; i < nargs; i++)
        {
            m3_argv[i] = JS_ToCString(ctx, argv[i + 1]);
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

};

void nx_init_wasm_(JSContext *ctx, JSValueConst native_obj)
{
    JS_SetPropertyFunctionList(ctx, native_obj, function_list, countof(function_list));
}

static JSValue nx_wasm_memory_new(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSValue obj = nx_wasm_memory_new_(ctx);
    if (JS_IsException(obj))
        return JS_EXCEPTION;

    nx_wasm_memory_t *data = nx_wasm_memory_get(ctx, obj);
    if (!data)
        return JS_EXCEPTION;

    IM3Memory mem = js_mallocz(ctx, sizeof(M3Memory));
    data->mem = mem;
    data->needs_free = true;
    data->is_shared = JS_ToBool(ctx, JS_GetPropertyStr(ctx, argv[0], "shared"));
    if (data->is_shared == -1)
        return JS_EXCEPTION;

    u32 initial;
    if (JS_ToUint32(ctx, &initial, JS_GetPropertyStr(ctx, argv[0], "initial")))
        return JS_EXCEPTION;

    u32 maxPages;
    if (JS_ToUint32(ctx, &maxPages, JS_GetPropertyStr(ctx, argv[0], "maximum")))
        return JS_EXCEPTION;

    mem->numPages = initial;
    mem->maxPages = maxPages ? maxPages : 65536;

    size_t numBytes = d_m3MemPageSize * initial;
    size_t numPreviousBytes = 0;
    void *newMem = m3_Realloc("Wasm Linear Memory", mem->mallocated, numBytes, numPreviousBytes);
    mem->mallocated = (M3MemoryHeader *)newMem;
    mem->mallocated->length = numBytes;

    // `runtime` and `maxStack` get set during import

    return obj;
}

// `Memory#buffer` getter function
static JSValue nx_wasm_memory_buffer_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_wasm_memory_t *data = nx_wasm_memory_get(ctx, this_val);
    if (!data)
    {
        return JS_EXCEPTION;
    }

    IM3Memory mem = data->mem;
    if (!mem)
    {
        JS_ThrowTypeError(ctx, "Memory not set");
        return JS_EXCEPTION;
    }

    M3MemoryHeader *mallocated = mem->mallocated;
    if (!mallocated)
    {
        JS_ThrowTypeError(ctx, "Memory not allocated");
        return JS_EXCEPTION;
    }

    size_t size = mallocated->length;
    uint8_t *memory = m3MemData(mallocated);

    JSValue buf = JS_NewArrayBuffer(ctx, memory, size, NULL, NULL, data->is_shared);
    if (JS_IsException(buf))
    {
        return JS_EXCEPTION;
    }
    // TODO: return same instance. invalidate when size changes
    return buf;
}

// `Table#get()` function
static JSValue nx_wasm_table_get_fn(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_wasm_table_t *data = nx_wasm_table_get(ctx, argv[0]);
    if (!data)
        return JS_EXCEPTION;

    u32 index;
    if (JS_ToUint32(ctx, &index, argv[1]))
        return JS_EXCEPTION;

    u32 size = *data->table_size;

    if (index >= size)
    {
        JS_ThrowRangeError(ctx, "WebAssembly.Table.get(): invalid index %u into funcref table of size %u", index, size);
        return JS_EXCEPTION;
    }

    IM3Function func = data->table[index];
    if (!func)
    {
        return JS_NULL;
    }

    return nx_wasm_exported_func_new(ctx, func);
}

// `Table#length` getter function
static JSValue nx_wasm_table_length_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_wasm_table_t *data = nx_wasm_table_get(ctx, this_val);
    if (!data)
    {
        return JS_EXCEPTION;
    }

    if (!data->table_size)
    {
        JS_ThrowTypeError(ctx, "Table size not set");
        return JS_EXCEPTION;
    }

    return JS_NewUint32(ctx, *data->table_size);
}

/* Initialize the `Memory` class */
static JSValue nx_wasm_init_memory_class(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSAtom atom;
    JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
    NX_DEF_GETTER(proto, "buffer", nx_wasm_memory_buffer_get);
    JS_FreeValue(ctx, proto);
    return JS_UNDEFINED;
}

/* Initialize the `Table` class */
static JSValue nx_wasm_init_table_class(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSAtom atom;
    JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
    // NX_DEF_FUNC(proto, "get", nx_wasm_table_get_fn, 1);
    NX_DEF_GETTER(proto, "length", nx_wasm_table_length_get);
    JS_FreeValue(ctx, proto);
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry init_function_list[] = {
    JS_CFUNC_DEF("wasmCallFunc", 1, nx_wasm_call_func),
    JS_CFUNC_DEF("wasmMemNew", 1, nx_wasm_memory_new),
    JS_CFUNC_DEF("wasmTableGet", 2, nx_wasm_table_get_fn),
    JS_CFUNC_DEF("wasmInitMemory", 1, nx_wasm_init_memory_class),
    JS_CFUNC_DEF("wasmInitTable", 1, nx_wasm_init_table_class),
};

void nx_init_wasm(JSContext *ctx, JSValueConst init_obj)
{
    JSRuntime *rt = JS_GetRuntime(ctx);

    /* WebAssembly.Global */
    JS_NewClassID(&nx_wasm_global_class_id);
    JSClassDef nx_wasm_global_class = {
        "WebAssembly.Global",
        .finalizer = finalizer_wasm_global,
    };
    JS_NewClass(rt, nx_wasm_global_class_id, &nx_wasm_global_class);

    /* WebAssembly.Memory */
    JS_NewClassID(&nx_wasm_memory_class_id);
    JSClassDef nx_wasm_memory_class = {
        "WebAssembly.Memory",
        .finalizer = finalizer_wasm_memory,
    };
    JS_NewClass(rt, nx_wasm_memory_class_id, &nx_wasm_memory_class);

    /* WebAssembly.Table */
    JS_NewClassID(&nx_wasm_table_class_id);
    JSClassDef nx_wasm_table_class = {
        "WebAssembly.Table",
        .finalizer = finalizer_wasm_table,
    };
    JS_NewClass(rt, nx_wasm_table_class_id, &nx_wasm_table_class);

    /* WebAssembly.Function */
    JS_NewClassID(&nx_wasm_exported_func_class_id);
    JSClassDef nx_wasm_exported_func_class = {
        "WebAssembly.Function",
        .finalizer = finalizer_wasm_exported_func,
    };
    JS_NewClass(rt, nx_wasm_exported_func_class_id, &nx_wasm_exported_func_class);

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

    JS_SetPropertyFunctionList(ctx, init_obj, init_function_list, countof(init_function_list));
}
