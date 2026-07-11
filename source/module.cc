#include "module.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unordered_map>

// ada's C API header has no extern "C" guard; the symbols are defined with C
// linkage (see source/url.cc), so wrap the include to match.
extern "C" {
#include <ada_c.h>
}

using namespace v8;

namespace {

// resolved-URL -> Module, so the same URL always returns the same instance
// (V8 requires referential stability; also handles import cycles).
std::unordered_map<std::string, Global<Module>> g_module_cache;
// Module identity hash -> resolved URL, to recover a referrer's base URL and to
// populate import.meta.url per module.
std::unordered_map<int, std::string> g_module_urls;
// The entrypoint module's URL (drives import.meta.main).
std::string g_entrypoint_url;

// Per-page importmap: page-base URL -> (bare specifier -> resolved target URL).
// Populated by nx_module_set_importmap; consulted by resolve_specifier_with_map
// when a bare specifier fails direct URL resolution. Targets are stored already
// resolved against the page base so lookup is O(1) and does not re-parse.
std::unordered_map<std::string,
                   std::unordered_map<std::string, std::string>>
    g_importmaps;
// Resolved-URL -> source text prefetched by the runtime (via `fetch()`). When
// present, load_module skips fopen and compiles from this text instead. Used
// for URL schemes the engine has no direct filesystem access to (e.g.
// brewser://, sdmc-scoped page assets), or when the runtime has already paid
// for the read via JS fetch and wants to avoid a second read.
std::unordered_map<std::string, std::string> g_prefetch_sources;
// Resolved-URL -> page-base URL that the module belongs to. Used by
// resolve_module_callback to look up which importmap applies when a module
// imports a bare specifier: the map for `g_module_page_base[referrer.url]`.
// Modules loaded outside a page context (e.g. the runtime entrypoint via
// nx_run_entry_module) are absent from this map.
std::unordered_map<std::string, std::string> g_module_page_base;

void register_module(Isolate *iso, Local<Module> module,
                     const std::string &url) {
	g_module_cache[url].Reset(iso, module);
	g_module_urls[module->GetIdentityHash()] = url;
}

// Safely convert a String::Utf8Value to std::string. `*v` can be null (e.g. on
// an OOM during the UTF-8 conversion), and std::string(nullptr, 0) is undefined
// behavior — so treat a null pointer as the empty string.
std::string utf8_to_string(const String::Utf8Value &v) {
	return *v ? std::string(*v, v.length()) : std::string();
}

// Read an entire file at `path` (a mounted devoptab URL, e.g. romfs:/x.js, or a
// host path). Returns a malloc'd NUL-terminated buffer (caller frees) or NULL.
char *read_module_file(const char *path, size_t *out_size) {
	FILE *f = fopen(path, "rb");
	if (!f)
		return NULL;
	// Determine the size; treat any seek/tell failure as an error so we never
	// compile a truncated module.
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return NULL;
	}
	long n = ftell(f);
	if (n < 0 || fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return NULL;
	}
	char *buf = (char *)malloc((size_t)n + 1);
	if (!buf) {
		fclose(f);
		return NULL;
	}
	size_t rd = fread(buf, 1, (size_t)n, f);
	// A short read (I/O error or unexpected truncation) must NOT silently yield
	// partial source — bail rather than compile an incomplete module.
	if (rd != (size_t)n || ferror(f)) {
		fclose(f);
		free(buf);
		return NULL;
	}
	fclose(f);
	buf[rd] = '\0';
	if (out_size)
		*out_size = rd;
	return buf;
}

// Resolve `specifier` against `base` (an absolute URL) into an absolute URL.
// Returns false for bare specifiers or on parse failure.
bool resolve_specifier(const std::string &base, const std::string &specifier,
                       std::string &out) {
	// Only relative ("./", "../"), absolute-path ("/x"), and absolute-URL
	// ("scheme:...") specifiers are supported; bare specifiers are rejected.
	bool is_relative =
	    specifier.rfind("./", 0) == 0 || specifier.rfind("../", 0) == 0;
	bool is_abs_path = !specifier.empty() && specifier[0] == '/';
	bool has_scheme = !is_relative && !is_abs_path &&
	                  specifier.find(':') != std::string::npos;
	if (!is_relative && !is_abs_path && !has_scheme)
		return false;

	ada_url url = ada_parse_with_base(specifier.c_str(), specifier.size(),
	                                  base.c_str(), base.size());
	if (!ada_is_valid(url)) {
		ada_free(url);
		return false;
	}
	ada_string href = ada_get_href(url);
	out.assign(href.data, href.length);
	ada_free(url);
	return true;
}

// Resolve `specifier` against `base`, with importmap fallback for bare
// specifiers. If direct URL resolution succeeds, use it (browser importmap
// spec: only bare specifiers consult the map). Otherwise consult the importmap
// registered for `page_base`. Returns false if neither path resolves.
bool resolve_specifier_with_map(const std::string &base,
                                const std::string &page_base,
                                const std::string &specifier,
                                std::string &out) {
	if (resolve_specifier(base, specifier, out))
		return true;

	if (page_base.empty())
		return false;
	auto page_it = g_importmaps.find(page_base);
	if (page_it == g_importmaps.end())
		return false;
	// Exact-match branch first — a specifier that appears verbatim as an
	// importmap key wins over any prefix that could also match.
	auto spec_it = page_it->second.find(specifier);
	if (spec_it != page_it->second.end()) {
		out = spec_it->second;
		return true;
	}
	// Prefix (packages-via-trailing-slash) match per importmap spec
	// https://html.spec.whatwg.org/multipage/webappapis.html#resolving-an-imports-match.
	// Any importmap key ending in `/` acts as a namespace prefix; a
	// specifier that begins with the key resolves to the mapped value
	// with the tail appended. Longest key wins so `foo/bar/` beats `foo/`
	// when both are registered. Registered targets are already resolved
	// against page_base (see nx_module_set_importmap), so tail append
	// produces an absolute URL directly.
	const std::string *best_key = nullptr;
	for (const auto &kv : page_it->second) {
		if (kv.first.empty() || kv.first.back() != '/') continue;
		if (specifier.rfind(kv.first, 0) != 0) continue; // startsWith
		if (best_key == nullptr || kv.first.size() > best_key->size())
			best_key = &kv.first;
	}
	if (best_key != nullptr) {
		out = page_it->second.at(*best_key) + specifier.substr(best_key->size());
		return true;
	}
	return false;
}

// Map a resolved module URL to a path that fopen() can open. Mounted devoptab
// schemes (romfs:/sdmc:/nxjs:) are opened as-is. `file://` (host test binary)
// is stripped to an absolute path.
std::string url_to_fs_path(const std::string &url) {
	if (url.rfind("file://", 0) == 0)
		return url.substr(7);
	if (url.rfind("file:", 0) == 0)
		return url.substr(5);
	return url;
}

// Compile (and cache) the module at the already-resolved absolute URL `url`.
// On failure, schedules a V8 exception and returns an empty MaybeLocal.
//
// `page_base` (may be empty) tags the loaded module with the page-scope whose
// importmap it should consult for bare specifiers. Runtime page-level module
// runs propagate this through the recursive resolver; entrypoint / filesystem
// modules pass empty.
//
// Source lookup precedence:
//   1. g_prefetch_sources[url] — set by nx_module_set_source when the runtime
//      has already fetched the body over its own (JS-side) `fetch`. Covers URL
//      schemes the engine's fopen() cannot reach (brewser://, http(s)://, and
//      sdmc:// resources living under paths the engine has no devoptab mount
//      for).
//   2. fopen(url_to_fs_path(url)) — the pre-existing filesystem path, used by
//      the entrypoint module and any relative import chain rooted at it.
MaybeLocal<Module> load_module(Isolate *iso, const std::string &url,
                               const std::string &page_base = "") {
	auto cached = g_module_cache.find(url);
	if (cached != g_module_cache.end())
		return cached->second.Get(iso);

	Local<String> source;
	auto pf_it = g_prefetch_sources.find(url);
	if (pf_it != g_prefetch_sources.end()) {
		const std::string &text = pf_it->second;
		if (!String::NewFromUtf8(iso, text.c_str(), NewStringType::kNormal,
		                         (int)text.size())
		         .ToLocal(&source)) {
			iso->ThrowException(Exception::Error(
			    nx_str(iso, "module source too large or invalid UTF-8")));
			return MaybeLocal<Module>();
		}
	} else {
		std::string path = url_to_fs_path(url);
		size_t len = 0;
		char *src = read_module_file(path.c_str(), &len);
		if (!src) {
			char msg[1024];
			snprintf(msg, sizeof(msg), "Cannot find module '%s'",
			         url.c_str());
			iso->ThrowException(Exception::Error(nx_str(iso, msg)));
			return MaybeLocal<Module>();
		}
		bool ok =
		    String::NewFromUtf8(iso, src, NewStringType::kNormal, (int)len)
		        .ToLocal(&source);
		free(src);
		if (!ok) {
			iso->ThrowException(Exception::Error(
			    nx_str(iso, "module source too large or invalid UTF-8")));
			return MaybeLocal<Module>();
		}
	}

	ScriptOrigin origin(nx_str(iso, url.c_str()), 0, 0, false, -1,
	                    Local<Value>(), false, false, true /* is_module */);
	ScriptCompiler::Source script_source(source, origin);
	Local<Module> module;
	if (!ScriptCompiler::CompileModule(iso, &script_source).ToLocal(&module))
		return MaybeLocal<Module>(); // CompileModule left an exception pending

	// Register BEFORE returning so import cycles resolve to this instance.
	register_module(iso, module, url);
	if (!page_base.empty())
		g_module_page_base[url] = page_base;
	return module;
}

// V8 static-import resolver: resolve `specifier` against the `referrer`'s URL.
// Bare specifiers consult the importmap registered for the referrer's page
// scope (via g_module_page_base); if the referrer belongs to no page scope
// (entrypoint / filesystem-loaded module), bare specifiers still fail.
MaybeLocal<Module> resolve_module_callback(Local<Context> context,
                                           Local<String> specifier,
                                           Local<FixedArray> import_assertions,
                                           Local<Module> referrer) {
	Isolate *iso = Isolate::GetCurrent();
	(void)context;
	(void)import_assertions; // JSON / asset modules are a future addition.

	auto it = g_module_urls.find(referrer->GetIdentityHash());
	std::string base = it != g_module_urls.end() ? it->second : g_entrypoint_url;
	auto pb_it = g_module_page_base.find(base);
	std::string page_base =
	    pb_it != g_module_page_base.end() ? pb_it->second : std::string();

	String::Utf8Value spec(iso, specifier);
	std::string spec_str = utf8_to_string(spec);
	std::string resolved;
	if (!resolve_specifier_with_map(base, page_base, spec_str, resolved)) {
		char msg[1024];
		snprintf(msg, sizeof(msg),
		         "Cannot find module '%s' imported from '%s' (bare specifiers "
		         "require an importmap for the page scope; relative and "
		         "absolute-URL specifiers do not)",
		         spec_str.c_str(), base.c_str());
		iso->ThrowException(Exception::Error(nx_str(iso, msg)));
		return MaybeLocal<Module>();
	}
	// Propagate page_base so child modules see the same importmap.
	return load_module(iso, resolved, page_base);
}

// Instantiate + evaluate a loaded module. Returns the evaluation result
// (a Promise; pending if the graph uses top-level await), or empty on failure.
MaybeLocal<Value> instantiate_and_evaluate(Isolate *iso,
                                           Local<Context> context,
                                           Local<Module> module) {
	if (module->InstantiateModule(context, resolve_module_callback).IsNothing())
		return MaybeLocal<Value>();
	return module->Evaluate(context);
}

void init_import_meta(Local<Context> context, Local<Module> module,
                      Local<Object> meta) {
	Isolate *iso = Isolate::GetCurrent();
	auto it = g_module_urls.find(module->GetIdentityHash());
	const std::string &url =
	    it != g_module_urls.end() ? it->second : g_entrypoint_url;
	meta->CreateDataProperty(context, nx_str(iso, "url"),
	                         nx_str(iso, url.c_str()))
	    .Check();
	meta->CreateDataProperty(context, nx_str(iso, "main"),
	                         Boolean::New(iso, url == g_entrypoint_url))
	    .Check();
}

// HostImportModuleDynamicallyCallback: backs `import()` for filesystem schemes.
// The whole graph loads synchronously (fopen), so resolve the returned promise
// with the module namespace once evaluation completes (chaining on the
// evaluation promise so top-level await in the imported graph is awaited).
MaybeLocal<Promise> dynamic_import_callback(
    Local<Context> context, Local<Data> host_defined_options,
    Local<Value> resource_name, Local<String> specifier,
    Local<FixedArray> import_attributes) {
	Isolate *iso = Isolate::GetCurrent();
	(void)host_defined_options;
	(void)import_attributes;

	Local<Promise::Resolver> resolver;
	if (!Promise::Resolver::New(context).ToLocal(&resolver))
		return MaybeLocal<Promise>(); // OOM: propagate as empty

	// Base URL: the importing module/script's resource name.
	std::string base;
	if (resource_name->IsString()) {
		String::Utf8Value rn(iso, resource_name);
		base = utf8_to_string(rn);
	}
	if (base.empty())
		base = g_entrypoint_url;

	String::Utf8Value spec(iso, specifier);
	std::string spec_str = utf8_to_string(spec);
	std::string resolved;

	// The importing script's page scope (if it is a page-level module) drives
	// bare-specifier resolution here too — dynamic import() from a
	// page-module inherits the page's importmap.
	auto pb_it = g_module_page_base.find(base);
	std::string page_base =
	    pb_it != g_module_page_base.end() ? pb_it->second : std::string();

	TryCatch try_catch(iso);
	bool failed = false;
	Local<Value> error;

	if (!resolve_specifier_with_map(base, page_base, spec_str, resolved)) {
		char msg[1024];
		snprintf(msg, sizeof(msg), "Cannot find module '%s' imported from '%s'",
		         spec_str.c_str(), base.c_str());
		error = Exception::Error(nx_str(iso, msg));
		failed = true;
	}

	Local<Module> module;
	if (!failed && !load_module(iso, resolved, page_base).ToLocal(&module)) {
		error = try_catch.Exception();
		failed = true;
	}

	Local<Value> eval_result;
	if (!failed &&
	    !instantiate_and_evaluate(iso, context, module).ToLocal(&eval_result)) {
		error = try_catch.Exception();
		failed = true;
	}

	if (failed) {
		if (error.IsEmpty())
			error = Exception::Error(nx_str(iso, "dynamic import failed"));
		resolver->Reject(context, error).Check();
		return resolver->GetPromise();
	}

	// `eval_result` is the module's evaluation promise (settled unless the
	// graph uses top-level await). `import()` must resolve to the namespace
	// once evaluation completes.
	Local<Promise> eval_promise = eval_result.As<Promise>();
	Local<Value> ns = module->GetModuleNamespace();
	if (eval_promise->State() == Promise::kFulfilled) {
		resolver->Resolve(context, ns).Check();
	} else if (eval_promise->State() == Promise::kRejected) {
		resolver->Reject(context, eval_promise->Result()).Check();
	} else {
		// Pending (top-level await): chain eval-completion -> namespace by
		// resolving with `eval_promise.then(() => ns)`. The namespace is
		// stashed in the then-callback's Data.
		Local<Object> data = Object::New(iso);
		data->Set(context, nx_str(iso, "ns"), ns).Check();
		Local<Function> on_ok =
		    Function::New(
		        context,
		        [](const FunctionCallbackInfo<Value> &info) {
			        Local<Context> ctx =
			            info.GetIsolate()->GetCurrentContext();
			        Local<Object> d = info.Data().As<Object>();
			        info.GetReturnValue().Set(
			            d->Get(ctx, nx_str(info.GetIsolate(), "ns"))
			                .ToLocalChecked());
		        },
		        data)
		        .ToLocalChecked();
		Local<Promise> mapped;
		if (eval_promise->Then(context, on_ok).ToLocal(&mapped))
			resolver->Resolve(context, mapped).Check();
		else
			resolver->Resolve(context, ns).Check();
	}
	return resolver->GetPromise();
}

} // namespace

// ---------------------------------------------------------------------------
// JS-callable page-module surface. Exposed on the `$` init object so runtime
// code (brewser-runtime canvas-runner, or any embedder driving per-page module
// execution) can wire an HTML `<script type="module">` and its `<script
// type="importmap">` sibling into V8's real module system without shelling out
// to a userland loader like SystemJS.
//
// Design contract (see also NXJS_PATCHES_NEEDED.md #105):
//   1. Runtime calls moduleSetImportmap(pageBase, mapJson) for each importmap
//      tag on the page. Multiple calls merge (last write wins per specifier).
//   2. Runtime walks the entry module's imports (JS-side scan), calls
//      moduleSetSource(resolvedUrl, source) for every URL it fetches. This
//      lets the engine's resolver skip fopen for scheme URLs it cannot open
//      itself (brewser://, http(s)://) and for source it has already read.
//   3. Runtime calls moduleRun(source, url, pageBase) with the inline module's
//      body (or fetched src). The returned Promise resolves when top-level
//      await settles.
//   4. On page navigation, runtime calls moduleClearPage(pageBase) to purge
//      the page's importmap, module cache entries, and prefetched sources.
// ---------------------------------------------------------------------------

namespace {

// Parse a JSON importmap of the shape `{"imports": {spec: target, ...}, ...}`
// and merge entries into g_importmaps[page_base]. Targets are resolved against
// page_base at registration time; specifier lookups at resolve time are then
// O(1) and never re-parse. `scopes` is accepted (permissive) but ignored for
// this pass; unscoped `imports` covers the common case (single-page demos and
// the whole Three.js ecosystem's importmap convention). Silently no-ops on
// malformed JSON — a broken importmap should not abort the page.
void nx_module_set_importmap(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	if (info.Length() < 2 || !info[0]->IsString() || !info[1]->IsString())
		return;

	String::Utf8Value base(iso, info[0]);
	String::Utf8Value json(iso, info[1]);
	std::string base_s = utf8_to_string(base);
	if (base_s.empty())
		return;

	Local<String> json_str = info[1].As<String>();
	Local<Value> parsed;
	if (!JSON::Parse(context, json_str).ToLocal(&parsed))
		return;
	if (!parsed->IsObject())
		return;
	Local<Object> map_obj = parsed.As<Object>();

	Local<Value> imports_val;
	if (!map_obj->Get(context, nx_str(iso, "imports")).ToLocal(&imports_val))
		return;
	if (!imports_val->IsObject())
		return;
	Local<Object> imports = imports_val.As<Object>();
	Local<Array> names;
	if (!imports->GetOwnPropertyNames(context).ToLocal(&names))
		return;

	auto &page_map = g_importmaps[base_s]; // create-or-merge
	uint32_t n = names->Length();
	for (uint32_t i = 0; i < n; i++) {
		Local<Value> key_val;
		if (!names->Get(context, i).ToLocal(&key_val))
			continue;
		if (!key_val->IsString())
			continue;
		Local<Value> val;
		if (!imports->Get(context, key_val).ToLocal(&val))
			continue;
		if (!val->IsString())
			continue;

		String::Utf8Value key_u(iso, key_val);
		String::Utf8Value val_u(iso, val);
		std::string k = utf8_to_string(key_u);
		std::string v = utf8_to_string(val_u);
		if (k.empty() || v.empty())
			continue;

		// Resolve target against page_base so bare-specifier lookups return a
		// fully-qualified URL. If the target is itself a bare identifier
		// (importmap chaining), store as-is; resolve_specifier will refuse it
		// at load time, which surfaces as a clear error.
		std::string resolved;
		if (resolve_specifier(base_s, v, resolved))
			page_map[k] = resolved;
		else
			page_map[k] = v;
	}
}

// Register `source` as the body for `url`. load_module consults this map
// before fopen. Idempotent (last write wins) so a repeat fetch during
// development or a re-invocation does not error.
void nx_module_set_source(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	if (info.Length() < 2 || !info[0]->IsString() || !info[1]->IsString())
		return;
	String::Utf8Value url(iso, info[0]);
	String::Utf8Value src(iso, info[1]);
	std::string url_s = utf8_to_string(url);
	if (url_s.empty())
		return;
	g_prefetch_sources[url_s] = utf8_to_string(src);
}

// Compile `source` as a module identified by `url`, tag it with `pageBase` so
// its bare-specifier imports consult the right importmap, InstantiateModule
// (recursively resolving), Evaluate, and return a Promise that mirrors the
// evaluation promise (fulfilled with the module namespace, rejected on any
// failure at compile/instantiate/evaluate).
void nx_module_run(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();

	Local<Promise::Resolver> resolver;
	if (!Promise::Resolver::New(context).ToLocal(&resolver))
		return;
	info.GetReturnValue().Set(resolver->GetPromise());

	if (info.Length() < 3 || !info[0]->IsString() || !info[1]->IsString() ||
	    !info[2]->IsString()) {
		resolver
		    ->Reject(context,
		             Exception::TypeError(nx_str(
		                 iso, "moduleRun(source, url, pageBase): 3 strings")))
		    .Check();
		return;
	}

	String::Utf8Value src(iso, info[0]);
	String::Utf8Value url(iso, info[1]);
	String::Utf8Value base(iso, info[2]);
	std::string url_s = utf8_to_string(url);
	std::string base_s = utf8_to_string(base);
	if (url_s.empty() || base_s.empty()) {
		resolver
		    ->Reject(context,
		             Exception::TypeError(
		                 nx_str(iso, "moduleRun: url and pageBase required")))
		    .Check();
		return;
	}

	// If this URL has already been compiled, `resolve_module_callback` would
	// return the cached instance; re-executing would either replay the module
	// (silently discarding new source) or throw a redeclaration error. Neither
	// is what the caller expects. Fail loudly so runtime code assigns unique
	// URLs per inline / re-run.
	if (g_module_cache.find(url_s) != g_module_cache.end()) {
		resolver
		    ->Reject(context,
		             Exception::Error(nx_str(
		                 iso, "moduleRun: url already registered")))
		    .Check();
		return;
	}

	TryCatch try_catch(iso);
	Local<String> source_v8 = info[0].As<String>();

	ScriptOrigin origin(nx_str(iso, url_s.c_str()), 0, 0, false, -1,
	                    Local<Value>(), false, false, true /* is_module */);
	ScriptCompiler::Source script_source(source_v8, origin);
	Local<Module> module;
	if (!ScriptCompiler::CompileModule(iso, &script_source).ToLocal(&module)) {
		resolver->Reject(context, try_catch.Exception()).Check();
		return;
	}
	register_module(iso, module, url_s);
	g_module_page_base[url_s] = base_s;

	if (module->InstantiateModule(context, resolve_module_callback)
	        .IsNothing()) {
		resolver->Reject(context, try_catch.Exception()).Check();
		return;
	}

	Local<Value> eval_result;
	if (!module->Evaluate(context).ToLocal(&eval_result)) {
		resolver->Reject(context, try_catch.Exception()).Check();
		return;
	}

	// `eval_result` is the module's evaluation promise (settled unless the
	// graph uses top-level await). Resolve our return promise to the module
	// namespace on fulfillment, propagate rejection otherwise. Chain via
	// `.then(() => ns)` when pending so TLA is awaited.
	Local<Promise> eval_promise = eval_result.As<Promise>();
	Local<Value> ns = module->GetModuleNamespace();
	if (eval_promise->State() == Promise::kFulfilled) {
		resolver->Resolve(context, ns).Check();
	} else if (eval_promise->State() == Promise::kRejected) {
		resolver->Reject(context, eval_promise->Result()).Check();
	} else {
		Local<Object> data = Object::New(iso);
		data->Set(context, nx_str(iso, "ns"), ns).Check();
		Local<Function> on_ok;
		if (Function::New(
		        context,
		        [](const FunctionCallbackInfo<Value> &info) {
			        Local<Context> ctx =
			            info.GetIsolate()->GetCurrentContext();
			        Local<Object> d = info.Data().As<Object>();
			        info.GetReturnValue().Set(
			            d->Get(ctx, nx_str(info.GetIsolate(), "ns"))
			                .ToLocalChecked());
		        },
		        data)
		        .ToLocal(&on_ok)) {
			Local<Promise> mapped;
			if (eval_promise->Then(context, on_ok).ToLocal(&mapped))
				resolver->Resolve(context, mapped).Check();
			else
				resolver->Resolve(context, ns).Check();
		} else {
			resolver->Resolve(context, ns).Check();
		}
	}
}

// Drop the page's importmap, and purge every module + prefetch entry tagged
// with `pageBase`. Modules loaded outside a page context (e.g. the runtime
// entrypoint via nx_run_entry_module) are absent from g_module_page_base and
// therefore untouched. Called on page navigation.
void nx_module_clear_page(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	if (info.Length() < 1 || !info[0]->IsString())
		return;
	String::Utf8Value base(iso, info[0]);
	std::string base_s = utf8_to_string(base);
	if (base_s.empty())
		return;

	g_importmaps.erase(base_s);

	for (auto it = g_module_page_base.begin();
	     it != g_module_page_base.end();) {
		if (it->second != base_s) {
			++it;
			continue;
		}
		const std::string &url = it->first;
		auto cache_it = g_module_cache.find(url);
		if (cache_it != g_module_cache.end()) {
			Local<Module> m = cache_it->second.Get(iso);
			g_module_urls.erase(m->GetIdentityHash());
			cache_it->second.Reset();
			g_module_cache.erase(cache_it);
		}
		g_prefetch_sources.erase(url);
		it = g_module_page_base.erase(it);
	}
}

} // namespace

void nx_init_modules(Isolate *iso) {
	iso->SetHostInitializeImportMetaObjectCallback(init_import_meta);
	iso->SetHostImportModuleDynamicallyCallback(dynamic_import_callback);
}

void nx_module_bindings(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "moduleSetImportmap", nx_module_set_importmap);
	NX_SET_FUNC(init_obj, "moduleSetSource", nx_module_set_source);
	NX_SET_FUNC(init_obj, "moduleRun", nx_module_run);
	NX_SET_FUNC(init_obj, "moduleClearPage", nx_module_clear_page);

	// Additionally publish as a durable global namespace so embedders whose
	// code loads AFTER the nx.js runtime — which captures `$` into a module
	// export and deletes the global at index.ts init — can still reach the
	// page-module surface. The nx.js runtime never touches `nxjsPageModules`,
	// so it survives for the isolate's lifetime.
	//
	// Access pattern from any embedder (post-nx.js-runtime-init):
	//   globalThis.nxjsPageModules.setImportmap(base, json)
	//   globalThis.nxjsPageModules.setSource(url, src)
	//   await globalThis.nxjsPageModules.run(src, url, base)
	//   globalThis.nxjsPageModules.clearPage(base)
	Local<Context> _ctx = iso->GetCurrentContext();
	Local<Object> mods_ns = Object::New(iso);
	NX_SET_FUNC(mods_ns, "setImportmap", nx_module_set_importmap);
	NX_SET_FUNC(mods_ns, "setSource", nx_module_set_source);
	NX_SET_FUNC(mods_ns, "run", nx_module_run);
	NX_SET_FUNC(mods_ns, "clearPage", nx_module_clear_page);
	_ctx->Global()
	    ->DefineOwnProperty(_ctx, nx_str(iso, "nxjsPageModules"), mods_ns,
	                        static_cast<PropertyAttribute>(DontEnum | DontDelete))
	    .Check();
}

bool nx_run_entry_module(Isolate *iso, Local<Context> context, const char *src,
                         size_t len, const char *name) {
	HandleScope scope(iso);
	TryCatch try_catch(iso);
	g_entrypoint_url = name;

	Local<String> source;
	if (!String::NewFromUtf8(iso, src, NewStringType::kNormal, (int)len)
	         .ToLocal(&source)) {
		nx_console_init(nx_ctx(iso));
		fprintf(stderr,
		        "Failed to load %s: source is %zu bytes, which exceeds V8's "
		        "maximum string length\n",
		        name, len);
		return false;
	}

	ScriptOrigin origin(nx_str(iso, name), 0, 0, false, -1, Local<Value>(),
	                    false, false, true /* is_module */);
	ScriptCompiler::Source script_source(source, origin);
	Local<Module> module;
	if (!ScriptCompiler::CompileModule(iso, &script_source).ToLocal(&module)) {
		nx_emit_error_event(iso, &try_catch);
		return false;
	}
	register_module(iso, module, name);

	Local<Value> result;
	if (!instantiate_and_evaluate(iso, context, module).ToLocal(&result)) {
		nx_emit_error_event(iso, &try_catch);
		return false;
	}

	// `result` is the evaluation promise. If the entrypoint graph uses
	// top-level await and rejects, surface it via the error path rather than
	// letting it become a silent unhandled rejection.
	if (result->IsPromise()) {
		Local<Promise> p = result.As<Promise>();
		if (p->State() == Promise::kRejected) {
			iso->ThrowException(p->Result());
			nx_emit_error_event(iso, &try_catch);
			return false;
		}
		if (p->State() == Promise::kPending) {
			Local<Function> on_reject;
			if (Function::New(context,
			                  [](const FunctionCallbackInfo<Value> &info) {
				                  Isolate *i = info.GetIsolate();
				                  TryCatch tc(i);
				                  i->ThrowException(info[0]);
				                  nx_emit_error_event(i, &tc);
			                  })
			        .ToLocal(&on_reject)) {
				Local<Promise> caught;
				if (!p->Catch(context, on_reject).ToLocal(&caught)) {
					// best-effort; ignore failure to attach the handler
				}
			}
		}
	}
	return true;
}

void nx_modules_teardown() {
	g_module_cache.clear(); // Global<Module> destructors release the handles
	g_module_urls.clear();
	g_entrypoint_url.clear();
	g_importmaps.clear();
	g_prefetch_sources.clear();
	g_module_page_base.clear();
}
