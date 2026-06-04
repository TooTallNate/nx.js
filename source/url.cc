// URL / URLSearchParams backed by the ada parser (ada.cpp / ada_c.h).
#include "error.h"
#include "types.h"
#include "wrap.h"
#include <stdlib.h>
#include <string.h>

// ada's C API header has no extern "C" guard; ada.cpp defines the symbols with
// C linkage, so wrap the include to match (url.cc is C++).
extern "C" {
#include "ada_c.h"
}

using namespace v8;

namespace {

enum iter_type {
	ITER_KEYS = 0,
	ITER_VALUES = 1,
	ITER_ENTRIES = 2,
};

// Shared between a URL object and its searchParams object. Freed only when
// BOTH halves are gone (each finalizer clears its half; the last one frees).
typedef struct {
	ada_url url;
	ada_url_search_params params;
	bool params_modified;
} nx_url_t;

typedef struct {
	iter_type type;
	union {
		ada_url_search_params_keys_iter keys;
		ada_url_search_params_values_iter values;
		ada_url_search_params_entries_iter entries;
	} it;
} nx_url_iter_t;

void free_url_half(nx_url_t *data) {
	if (data->url) {
		ada_free(data->url);
		data->url = NULL;
	}
	if (!data->params)
		free(data); // params half already gone
}

void free_params_half(nx_url_t *data) {
	if (data->params) {
		ada_free_search_params(data->params);
		data->params = NULL;
	}
	if (!data->url)
		free(data); // url half already gone
}

void free_iter(nx_url_iter_t *data) {
	if (data->type == ITER_KEYS)
		ada_free_search_params_keys_iter(data->it.keys);
	else if (data->type == ITER_VALUES)
		ada_free_search_params_values_iter(data->it.values);
	else if (data->type == ITER_ENTRIES)
		ada_free_search_params_entries_iter(data->it.entries);
	free(data);
}

nx_url_t *get_url(Local<Value> v) { return nx::Unwrap<nx_url_t>(v); }

// Helper to build an ada_url from input[, base].
ada_url parse_input(Isolate *iso, const FunctionCallbackInfo<Value> &info,
                    String::Utf8Value &input) {
	if (info.Length() == 2 && !info[1]->IsUndefined()) {
		String::Utf8Value base(iso, info[1]);
		return ada_parse_with_base(*input, input.length(), *base,
		                           base.length());
	}
	return ada_parse(*input, input.length());
}

void nx_url_can_parse(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value input(iso, info[0]);
	ada_url url = parse_input(iso, info, input);
	info.GetReturnValue().Set(Boolean::New(iso, ada_is_valid(url)));
	ada_free(url);
}

void nx_url_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value input(iso, info[0]);
	ada_url url = parse_input(iso, info, input);
	if (!ada_is_valid(url)) {
		ada_free(url);
		nx_throw(iso, "not a valid URL");
		return;
	}
	nx_url_t *data = (nx_url_t *)calloc(1, sizeof(nx_url_t));
	data->url = url;
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_url_t>(iso, obj, data, free_url_half);
	info.GetReturnValue().Set(obj);
}

// String getter from an ada_string accessor.
Local<String> ada_str(Isolate *iso, ada_string val) {
	return String::NewFromUtf8(iso, val.data, NewStringType::kNormal,
	                           (int)val.length)
	    .ToLocalChecked();
}

#define DEFINE_GETTER_SETTER(NAME)                                             \
	void nx_url_get_##NAME(const FunctionCallbackInfo<Value> &info) {          \
		Isolate *iso = info.GetIsolate();                                      \
		nx_url_t *data = get_url(info.This());                                 \
		if (!data)                                                             \
			return;                                                            \
		info.GetReturnValue().Set(ada_str(iso, ada_get_##NAME(data->url)));    \
	}                                                                          \
	void nx_url_set_##NAME(const FunctionCallbackInfo<Value> &info) {          \
		Isolate *iso = info.GetIsolate();                                      \
		nx_url_t *data = get_url(info.This());                                 \
		if (!data)                                                             \
			return;                                                            \
		String::Utf8Value val(iso, info[0]);                                   \
		ada_set_##NAME(data->url, *val, val.length());                         \
	}

DEFINE_GETTER_SETTER(hash)
DEFINE_GETTER_SETTER(host)
DEFINE_GETTER_SETTER(hostname)
DEFINE_GETTER_SETTER(password)
DEFINE_GETTER_SETTER(pathname)
DEFINE_GETTER_SETTER(port)
DEFINE_GETTER_SETTER(protocol)
DEFINE_GETTER_SETTER(username)

void nx_url_get_search(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_url_t *data = get_url(info.This());
	if (!data)
		return;
	if (data->params && data->params_modified) {
		ada_owned_string val = ada_search_params_to_string(data->params);
		Local<String> str;
		if (val.length == 0) {
			str = nx_str(iso, "");
		} else {
			char *tmp = (char *)malloc(val.length + 1);
			tmp[0] = '?';
			memcpy(tmp + 1, val.data, val.length);
			str = String::NewFromUtf8(iso, tmp, NewStringType::kNormal,
			                          (int)val.length + 1)
			          .ToLocalChecked();
			free(tmp);
		}
		ada_free_owned_string(val);
		info.GetReturnValue().Set(str);
	} else {
		info.GetReturnValue().Set(ada_str(iso, ada_get_search(data->url)));
	}
}

void nx_url_set_search(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_url_t *data = get_url(info.This());
	if (!data)
		return;
	String::Utf8Value val(iso, info[0]);
	if (data->params) {
		ada_free_search_params(data->params);
		data->params = ada_parse_search_params(*val, val.length());
	}
	ada_set_search(data->url, *val, val.length());
	data->params_modified = false;
}

void nx_url_get_href(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_url_t *data = get_url(info.This());
	if (!data)
		return;
	if (data->params) {
		ada_owned_string val = ada_search_params_to_string(data->params);
		ada_set_search(data->url, val.data, val.length);
		ada_free_owned_string(val);
	}
	info.GetReturnValue().Set(ada_str(iso, ada_get_href(data->url)));
}

void nx_url_set_href(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_url_t *data = get_url(info.This());
	if (!data)
		return;
	String::Utf8Value val(iso, info[0]);
	ada_set_href(data->url, *val, val.length());
	if (data->params) {
		ada_free_search_params(data->params);
		ada_string search_val = ada_get_search(data->url);
		data->params =
		    ada_parse_search_params(search_val.data, search_val.length);
	}
	data->params_modified = false;
}

void nx_url_get_origin(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_url_t *data = get_url(info.This());
	if (!data)
		return;
	ada_owned_string val = ada_get_origin(data->url);
	info.GetReturnValue().Set(
	    String::NewFromUtf8(iso, val.data, NewStringType::kNormal,
	                        (int)val.length)
	        .ToLocalChecked());
	ada_free_owned_string(val);
}

Local<Object> proto_of(Isolate *iso, const FunctionCallbackInfo<Value> &info) {
	Local<Context> context = iso->GetCurrentContext();
	return info[0]
	    .As<Object>()
	    ->Get(context, nx_str(iso, "prototype"))
	    .ToLocalChecked()
	    .As<Object>();
}

void nx_url_init(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Object> proto = proto_of(iso, info);
	NX_DEF_GET(proto, "origin", nx_url_get_origin);
	NX_DEF_GETSET(proto, "hash", nx_url_get_hash, nx_url_set_hash);
	NX_DEF_GETSET(proto, "host", nx_url_get_host, nx_url_set_host);
	NX_DEF_GETSET(proto, "hostname", nx_url_get_hostname, nx_url_set_hostname);
	NX_DEF_GETSET(proto, "href", nx_url_get_href, nx_url_set_href);
	NX_DEF_GETSET(proto, "password", nx_url_get_password, nx_url_set_password);
	NX_DEF_GETSET(proto, "pathname", nx_url_get_pathname, nx_url_set_pathname);
	NX_DEF_GETSET(proto, "port", nx_url_get_port, nx_url_set_port);
	NX_DEF_GETSET(proto, "protocol", nx_url_get_protocol, nx_url_set_protocol);
	NX_DEF_GETSET(proto, "search", nx_url_get_search, nx_url_set_search);
	NX_DEF_GETSET(proto, "username", nx_url_get_username, nx_url_set_username);
	Local<Object> cls = info[0].As<Object>();
	NX_DEF_FUNC(cls, "canParse", nx_url_can_parse, 1);
}

void nx_url_search_params_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_url_t *data;
	bool shared = false;
	if (info.Length() == 2 && !info[1]->IsUndefined()) {
		// searchParams on an existing URL: share its nx_url_t.
		data = get_url(info[1]);
		shared = true;
	} else {
		data = (nx_url_t *)calloc(1, sizeof(nx_url_t));
	}
	if (!data)
		return;
	String::Utf8Value input(iso, info[0]);
	data->params = ada_parse_search_params(*input, input.length());
	Local<Object> obj = nx::NewWrapped(iso);
	// If shared, the URL half owns the struct lifetime guard too; both
	// finalizers cooperate via free_*_half.
	nx::Wrap<nx_url_t>(iso, obj, data, free_params_half);
	(void)shared;
	info.GetReturnValue().Set(obj);
}

nx_url_t *get_params(Local<Value> v) { return nx::Unwrap<nx_url_t>(v); }

void nx_url_search_params_get_size(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_url_t *data = get_params(info.This());
	if (!data)
		return;
	info.GetReturnValue().Set(
	    Integer::NewFromUnsigned(iso, ada_search_params_size(data->params)));
}

void nx_url_search_params_append(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_url_t *data = get_params(info.This());
	if (!data)
		return;
	String::Utf8Value key(iso, info[0]);
	String::Utf8Value value(iso, info[1]);
	ada_search_params_append(data->params, *key, key.length(), *value,
	                         value.length());
	data->params_modified = true;
}

void nx_url_search_params_delete(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_url_t *data = get_params(info.This());
	if (!data)
		return;
	String::Utf8Value key(iso, info[0]);
	if (info.Length() == 2 && info[1]->IsString()) {
		String::Utf8Value value(iso, info[1]);
		ada_search_params_remove_value(data->params, *key, key.length(),
		                               *value, value.length());
	} else {
		ada_search_params_remove(data->params, *key, key.length());
	}
	data->params_modified = true;
}

void nx_url_search_params_get(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_url_t *data = get_params(info.This());
	if (!data)
		return;
	String::Utf8Value key(iso, info[0]);
	ada_string val = ada_search_params_get(data->params, *key, key.length());
	if (val.data == NULL) {
		info.GetReturnValue().SetNull();
		return;
	}
	info.GetReturnValue().Set(ada_str(iso, val));
}

void nx_url_search_params_get_all(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	nx_url_t *data = get_params(info.This());
	if (!data)
		return;
	String::Utf8Value key(iso, info[0]);
	ada_strings vals =
	    ada_search_params_get_all(data->params, *key, key.length());
	size_t len = ada_strings_size(vals);
	Local<Array> arr = Array::New(iso, len);
	for (size_t i = 0; i < len; i++) {
		arr->Set(context, (uint32_t)i, ada_str(iso, ada_strings_get(vals, i)))
		    .Check();
	}
	ada_free_strings(vals);
	info.GetReturnValue().Set(arr);
}

void nx_url_search_params_has(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_url_t *data = get_params(info.This());
	if (!data)
		return;
	String::Utf8Value key(iso, info[0]);
	info.GetReturnValue().Set(Boolean::New(
	    iso, ada_search_params_has(data->params, *key, key.length())));
}

void nx_url_search_params_set(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_url_t *data = get_params(info.This());
	if (!data)
		return;
	String::Utf8Value key(iso, info[0]);
	String::Utf8Value value(iso, info[1]);
	ada_search_params_set(data->params, *key, key.length(), *value,
	                      value.length());
	data->params_modified = true;
}

void nx_url_search_params_sort(const FunctionCallbackInfo<Value> &info) {
	nx_url_t *data = get_params(info.This());
	if (!data)
		return;
	ada_search_params_sort(data->params);
	data->params_modified = true;
}

void nx_url_search_params_to_string(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_url_t *data = get_params(info.This());
	if (!data)
		return;
	ada_owned_string val = ada_search_params_to_string(data->params);
	info.GetReturnValue().Set(
	    String::NewFromUtf8(iso, val.data, NewStringType::kNormal,
	                        (int)val.length)
	        .ToLocalChecked());
	ada_free_owned_string(val);
}

void nx_url_search_params_init(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Object> proto = proto_of(iso, info);
	NX_DEF_GET(proto, "size", nx_url_search_params_get_size);
	NX_DEF_FUNC(proto, "append", nx_url_search_params_append, 2);
	NX_DEF_FUNC(proto, "delete", nx_url_search_params_delete, 1);
	NX_DEF_FUNC(proto, "get", nx_url_search_params_get, 1);
	NX_DEF_FUNC(proto, "getAll", nx_url_search_params_get_all, 1);
	NX_DEF_FUNC(proto, "has", nx_url_search_params_has, 1);
	NX_DEF_FUNC(proto, "set", nx_url_search_params_set, 2);
	NX_DEF_FUNC(proto, "sort", nx_url_search_params_sort, 0);
	NX_DEF_FUNC(proto, "toString", nx_url_search_params_to_string, 0);
}

void nx_url_search_params_iterator(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	nx_url_t *params_data = get_params(info[0]);
	if (!params_data)
		return;
	uint32_t type = 0;
	if (!info[1]->Uint32Value(context).To(&type))
		return;
	nx_url_iter_t *data = (nx_url_iter_t *)calloc(1, sizeof(nx_url_iter_t));
	data->type = (iter_type)type;
	if (type == ITER_KEYS) {
		data->it.keys = ada_search_params_get_keys(params_data->params);
	} else if (type == ITER_VALUES) {
		data->it.values = ada_search_params_get_values(params_data->params);
	} else if (type == ITER_ENTRIES) {
		data->it.entries = ada_search_params_get_entries(params_data->params);
	} else {
		free(data);
		nx_throw(iso, "Invalid URLSearchParams iterator type");
		return;
	}
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_url_iter_t>(iso, obj, data, free_iter);
	info.GetReturnValue().Set(obj);
}

void nx_url_search_params_iterator_next(
    const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	nx_url_iter_t *data = nx::Unwrap<nx_url_iter_t>(info[0]);
	if (!data)
		return;
	if (data->type == ITER_KEYS) {
		if (ada_search_params_keys_iter_has_next(data->it.keys)) {
			info.GetReturnValue().Set(
			    ada_str(iso, ada_search_params_keys_iter_next(data->it.keys)));
		}
	} else if (data->type == ITER_VALUES) {
		if (ada_search_params_values_iter_has_next(data->it.values)) {
			info.GetReturnValue().Set(ada_str(
			    iso, ada_search_params_values_iter_next(data->it.values)));
		}
	} else if (data->type == ITER_ENTRIES) {
		if (ada_search_params_entries_iter_has_next(data->it.entries)) {
			ada_string_pair pair =
			    ada_search_params_entries_iter_next(data->it.entries);
			Local<Array> arr = Array::New(iso, 2);
			arr->Set(context, 0, ada_str(iso, pair.key)).Check();
			arr->Set(context, 1, ada_str(iso, pair.value)).Check();
			info.GetReturnValue().Set(arr);
		}
	}
}

} // namespace

void nx_init_url(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "urlNew", nx_url_new);
	NX_SET_FUNC(init_obj, "urlInit", nx_url_init);
	NX_SET_FUNC(init_obj, "urlSearchNew", nx_url_search_params_new);
	NX_SET_FUNC(init_obj, "urlSearchInit", nx_url_search_params_init);
	NX_SET_FUNC(init_obj, "urlSearchIterator", nx_url_search_params_iterator);
	NX_SET_FUNC(init_obj, "urlSearchIteratorNext",
	            nx_url_search_params_iterator_next);
}
