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
MaybeLocal<Module> load_module(Isolate *iso, const std::string &url) {
	auto cached = g_module_cache.find(url);
	if (cached != g_module_cache.end())
		return cached->second.Get(iso);

	std::string path = url_to_fs_path(url);
	size_t len = 0;
	char *src = read_module_file(path.c_str(), &len);
	if (!src) {
		char msg[1024];
		snprintf(msg, sizeof(msg), "Cannot find module '%s'", url.c_str());
		iso->ThrowException(Exception::Error(nx_str(iso, msg)));
		return MaybeLocal<Module>();
	}

	Local<String> source;
	bool ok = String::NewFromUtf8(iso, src, NewStringType::kNormal, (int)len)
	              .ToLocal(&source);
	free(src);
	if (!ok) {
		iso->ThrowException(Exception::Error(
		    nx_str(iso, "module source too large or invalid UTF-8")));
		return MaybeLocal<Module>();
	}

	ScriptOrigin origin(nx_str(iso, url.c_str()), 0, 0, false, -1,
	                    Local<Value>(), false, false, true /* is_module */);
	ScriptCompiler::Source script_source(source, origin);
	Local<Module> module;
	if (!ScriptCompiler::CompileModule(iso, &script_source).ToLocal(&module))
		return MaybeLocal<Module>(); // CompileModule left an exception pending

	// Register BEFORE returning so import cycles resolve to this instance.
	register_module(iso, module, url);
	return module;
}

// V8 static-import resolver: resolve `specifier` against the `referrer`'s URL.
MaybeLocal<Module> resolve_module_callback(Local<Context> context,
                                           Local<String> specifier,
                                           Local<FixedArray> import_assertions,
                                           Local<Module> referrer) {
	Isolate *iso = Isolate::GetCurrent();
	(void)context;
	(void)import_assertions; // JSON / asset modules are a future addition.

	auto it = g_module_urls.find(referrer->GetIdentityHash());
	std::string base = it != g_module_urls.end() ? it->second : g_entrypoint_url;

	String::Utf8Value spec(iso, specifier);
	std::string spec_str = utf8_to_string(spec);
	std::string resolved;
	if (!resolve_specifier(base, spec_str, resolved)) {
		char msg[1024];
		snprintf(msg, sizeof(msg),
		         "Cannot find module '%s' imported from '%s' (only relative "
		         "and absolute-URL specifiers are supported)",
		         spec_str.c_str(), base.c_str());
		iso->ThrowException(Exception::Error(nx_str(iso, msg)));
		return MaybeLocal<Module>();
	}
	return load_module(iso, resolved);
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

	TryCatch try_catch(iso);
	bool failed = false;
	Local<Value> error;

	if (!resolve_specifier(base, spec_str, resolved)) {
		char msg[1024];
		snprintf(msg, sizeof(msg), "Cannot find module '%s' imported from '%s'",
		         spec_str.c_str(), base.c_str());
		error = Exception::Error(nx_str(iso, msg));
		failed = true;
	}

	Local<Module> module;
	if (!failed && !load_module(iso, resolved).ToLocal(&module)) {
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

void nx_init_modules(Isolate *iso) {
	iso->SetHostInitializeImportMetaObjectCallback(init_import_meta);
	iso->SetHostImportModuleDynamicallyCallback(dynamic_import_callback);
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
}
