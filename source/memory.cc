#include "types.h"

using namespace v8;

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
	info.GetReturnValue().Set(obj);
}

} // namespace

void nx_init_memory(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "memoryUsage", nx_memory_usage);
}
