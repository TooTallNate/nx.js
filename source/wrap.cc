#include "wrap.h"

using namespace v8;

namespace nx {

// One cached ObjectTemplate (1 internal field) per isolate, stored in the
// isolate's embedder data slot 1 via an Eternal handle held in a static.
namespace {
struct TemplateCache {
	Eternal<ObjectTemplate> tmpl;
};
TemplateCache *g_cache = nullptr; // single-isolate runtime (see BINDINGS.md §0)
} // namespace

Local<Object> NewWrapped(Isolate *iso) {
	if (g_cache == nullptr) {
		g_cache = new TemplateCache();
		Local<ObjectTemplate> t = ObjectTemplate::New(iso);
		t->SetInternalFieldCount(1);
		g_cache->tmpl.Set(iso, t);
	}
	Local<Context> ctx = iso->GetCurrentContext();
	return g_cache->tmpl.Get(iso)->NewInstance(ctx).ToLocalChecked();
}

} // namespace nx
