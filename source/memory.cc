#include "types.h"
#include <malloc.h>

using namespace v8;

// newlib heap bounds (set up by libnx / hbloader). The span between them is
// the TOTAL native malloc capacity of the process — every native allocation
// (V8 heap slabs, JIT arena, Skia surfaces, libuv stacks, zstd windows,
// ArrayBuffer backings, ...) competes inside it.
extern "C" char *fake_heap_start;
extern "C" char *fake_heap_end;

namespace {

// Re-expressed via V8's HeapStatistics (replaces QuickJS JS_ComputeMemoryUsage).
void nx_memory_usage(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	HeapStatistics stats;
	iso->GetHeapStatistics(&stats);

	Local<Object> obj = Object::New(iso);
	auto set = [&](const char *k, size_t v) {
		obj->Set(context, nx_str(iso, k),
		         Number::New(iso, static_cast<double>(v)))
		    .Check();
	};
	set("totalHeapSize", stats.total_heap_size());
	set("totalHeapSizeExecutable", stats.total_heap_size_executable());
	set("totalPhysicalSize", stats.total_physical_size());
	set("totalAvailableSize", stats.total_available_size());
	set("usedHeapSize", stats.used_heap_size());
	set("heapSizeLimit", stats.heap_size_limit());
	set("mallocedMemory", stats.malloced_memory());
	set("peakMallocedMemory", stats.peak_malloced_memory());
	set("numberOfNativeContexts", stats.number_of_native_contexts());
	set("numberOfDetachedContexts", stats.number_of_detached_contexts());
	// Bytes of off-V8-heap memory (ArrayBuffer backing stores etc.) V8 knows
	// it is keeping alive. Large values here with a small usedHeapSize mean
	// native memory is held by not-yet-collected JS objects.
	set("externalMemory", (size_t)stats.external_memory());

	// Native (newlib) heap statistics. nativeHeapTotal is the process's whole
	// malloc capacity; nativeHeapUsed/nativeHeapFree reflect the allocator's
	// current arena (the arena can still grow until total is reached).
	struct mallinfo mi = mallinfo();
	set("nativeHeapTotal", (size_t)(fake_heap_end - fake_heap_start));
	set("nativeHeapArena", (size_t)mi.arena);
	set("nativeHeapUsed", (size_t)mi.uordblks);
	set("nativeHeapFree", (size_t)mi.fordblks);
	info.GetReturnValue().Set(obj);
}

} // namespace

void nx_init_memory(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "memoryUsage", nx_memory_usage);
}
