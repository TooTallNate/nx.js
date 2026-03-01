#include <malloc.h>
#include <unistd.h>

#include "error.h"
#include "memory.h"

// Defined by libsysbase (devkitPro) — the upper bound of the heap region.
// Set by libnx's __libnx_initheap() to (heap_base + heap_size).
extern char *fake_heap_end;

static JSValue nx_memory_usage(JSContext *ctx, JSValueConst this_val, int argc,
							   JSValueConst *argv) {
	JSRuntime *rt = JS_GetRuntime(ctx);
	JSMemoryUsage stats;
	JS_ComputeMemoryUsage(rt, &stats);

	struct mallinfo mi = mallinfo();

	// Available memory = free space in malloc's free list
	//                   + unsbrk'd space between current break and heap end
	char *brk = sbrk(0);
	size_t unsbrked = (brk != (char *)-1 && fake_heap_end > brk)
						  ? (size_t)(fake_heap_end - brk)
						  : 0;
	size_t available = (size_t)mi.fordblks + unsbrked;

	u64 total_memory = 0;
	u64 used_memory = 0;

	Result rc = svcGetInfo(&total_memory, InfoType_TotalMemorySize,
						   CUR_PROCESS_HANDLE, 0);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "svcGetInfo(TotalMemorySize)");
	}

	rc = svcGetInfo(&used_memory, InfoType_UsedMemorySize, CUR_PROCESS_HANDLE,
					0);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "svcGetInfo(UsedMemorySize)");
	}

	JSValue obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, obj, "rss",
					  JS_NewFloat64(ctx, (double)mi.uordblks));
	JS_SetPropertyStr(ctx, obj, "heapTotal",
					  JS_NewFloat64(ctx, (double)stats.memory_used_size));
	JS_SetPropertyStr(ctx, obj, "heapUsed",
					  JS_NewFloat64(ctx, (double)stats.malloc_size));
	JS_SetPropertyStr(ctx, obj, "totalSystemMemory",
					  JS_NewFloat64(ctx, (double)total_memory));
	JS_SetPropertyStr(ctx, obj, "usedSystemMemory",
					  JS_NewFloat64(ctx, (double)used_memory));
	JS_SetPropertyStr(ctx, obj, "availableMemory",
					  JS_NewFloat64(ctx, (double)available));
	return obj;
}

static JSValue nx_available_memory(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	struct mallinfo mi = mallinfo();

	// Available memory = free space in malloc's free list
	//                   + unsbrk'd space between current break and heap end
	char *brk = sbrk(0);
	size_t unsbrked = (brk != (char *)-1 && fake_heap_end > brk)
						  ? (size_t)(fake_heap_end - brk)
						  : 0;

	return JS_NewFloat64(ctx, (double)((size_t)mi.fordblks + unsbrked));
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("memoryUsage", 0, nx_memory_usage),
	JS_CFUNC_DEF("availableMemory", 0, nx_available_memory),
};

void nx_init_memory(JSContext *ctx, JSValueConst init_obj) {
	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
