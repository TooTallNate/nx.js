#include "memory.h"

static JSValue nx_memory_usage(JSContext *ctx, JSValueConst this_val, int argc,
							   JSValueConst *argv) {
	JSRuntime *rt = JS_GetRuntime(ctx);
	JSMemoryUsage stats;
	JS_ComputeMemoryUsage(rt, &stats);

	JSValue obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, obj, "malloc_size",
					  JS_NewInt64(ctx, stats.malloc_size));
	JS_SetPropertyStr(ctx, obj, "malloc_limit",
					  JS_NewInt64(ctx, stats.malloc_limit));
	JS_SetPropertyStr(ctx, obj, "memory_used_size",
					  JS_NewInt64(ctx, stats.memory_used_size));
	JS_SetPropertyStr(ctx, obj, "malloc_count",
					  JS_NewInt64(ctx, stats.malloc_count));
	JS_SetPropertyStr(ctx, obj, "memory_used_count",
					  JS_NewInt64(ctx, stats.memory_used_count));
	JS_SetPropertyStr(ctx, obj, "atom_count",
					  JS_NewInt64(ctx, stats.atom_count));
	JS_SetPropertyStr(ctx, obj, "atom_size",
					  JS_NewInt64(ctx, stats.atom_size));
	JS_SetPropertyStr(ctx, obj, "str_count",
					  JS_NewInt64(ctx, stats.str_count));
	JS_SetPropertyStr(ctx, obj, "str_size",
					  JS_NewInt64(ctx, stats.str_size));
	JS_SetPropertyStr(ctx, obj, "obj_count",
					  JS_NewInt64(ctx, stats.obj_count));
	JS_SetPropertyStr(ctx, obj, "obj_size",
					  JS_NewInt64(ctx, stats.obj_size));
	JS_SetPropertyStr(ctx, obj, "prop_count",
					  JS_NewInt64(ctx, stats.prop_count));
	JS_SetPropertyStr(ctx, obj, "prop_size",
					  JS_NewInt64(ctx, stats.prop_size));
	JS_SetPropertyStr(ctx, obj, "shape_count",
					  JS_NewInt64(ctx, stats.shape_count));
	JS_SetPropertyStr(ctx, obj, "shape_size",
					  JS_NewInt64(ctx, stats.shape_size));
	JS_SetPropertyStr(ctx, obj, "js_func_count",
					  JS_NewInt64(ctx, stats.js_func_count));
	JS_SetPropertyStr(ctx, obj, "js_func_size",
					  JS_NewInt64(ctx, stats.js_func_size));
	JS_SetPropertyStr(ctx, obj, "js_func_code_size",
					  JS_NewInt64(ctx, stats.js_func_code_size));
	JS_SetPropertyStr(ctx, obj, "js_func_pc2line_count",
					  JS_NewInt64(ctx, stats.js_func_pc2line_count));
	JS_SetPropertyStr(ctx, obj, "js_func_pc2line_size",
					  JS_NewInt64(ctx, stats.js_func_pc2line_size));
	JS_SetPropertyStr(ctx, obj, "c_func_count",
					  JS_NewInt64(ctx, stats.c_func_count));
	JS_SetPropertyStr(ctx, obj, "array_count",
					  JS_NewInt64(ctx, stats.array_count));
	JS_SetPropertyStr(ctx, obj, "fast_array_count",
					  JS_NewInt64(ctx, stats.fast_array_count));
	JS_SetPropertyStr(ctx, obj, "fast_array_elements",
					  JS_NewInt64(ctx, stats.fast_array_elements));
	JS_SetPropertyStr(ctx, obj, "binary_object_count",
					  JS_NewInt64(ctx, stats.binary_object_count));
	JS_SetPropertyStr(ctx, obj, "binary_object_size",
					  JS_NewInt64(ctx, stats.binary_object_size));
	return obj;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("memoryUsage", 0, nx_memory_usage),
};

void nx_init_memory(JSContext *ctx, JSValueConst init_obj) {
	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
