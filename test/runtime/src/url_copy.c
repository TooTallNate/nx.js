#include "types.h"
#include <quickjs.h>
void nx_init_url(JSContext *ctx, JSValueConst init_obj);
#include "ada_c.h"

#define STR(NAME, INDEX)                                                       \
	size_t NAME##_length;                                                      \
	const char *NAME = JS_ToCStringLen(ctx, &NAME##_length, argv[INDEX]);      \
	if (!NAME) {                                                               \
		return JS_EXCEPTION;                                                   \
	}

enum nx_url_search_params_iterator_type {
	NX_URL_SEARCH_PARAMS_ITERATOR_TYPE_KEYS,
	NX_URL_SEARCH_PARAMS_ITERATOR_TYPE_VALUES,
	NX_URL_SEARCH_PARAMS_ITERATOR_TYPE_ENTRIES
};

typedef struct {
	ada_url url;
	ada_url_search_params params;
	bool params_modified;
} nx_url_t;

typedef struct {
	enum nx_url_search_params_iterator_type type;
	union {
		ada_url_search_params_keys_iter keys;
		ada_url_search_params_values_iter values;
		ada_url_search_params_entries_iter entries;
	} it;
} nx_url_search_params_iterator_t;

static JSClassID nx_url_class_id;
static JSClassID nx_url_search_params_class_id;
static JSClassID nx_url_search_params_iterator_class_id;

static void finalizer_url(JSRuntime *rt, JSValue val) {
	// printf("finalizer_url\n");
	nx_url_t *data = JS_GetOpaque(val, nx_url_class_id);
	if (data) {
		if (data->url) {
			ada_free(data->url);
			data->url = NULL;
		}
		if (!data->params) {
			js_free_rt(rt, data);
		}
	}
}

static void finalizer_url_search_params(JSRuntime *rt, JSValue val) {
	// printf("finalizer_url_search_params\n");
	nx_url_t *data = JS_GetOpaque(val, nx_url_search_params_class_id);
	if (data) {
		if (data->params) {
			ada_free_search_params(data->params);
			data->params = NULL;
		}
		if (!data->url) {
			js_free_rt(rt, data);
		}
	}
}

static void finalizer_url_search_params_iterator(JSRuntime *rt, JSValue val) {
	nx_url_search_params_iterator_t *data =
		JS_GetOpaque(val, nx_url_search_params_iterator_class_id);
	if (data) {
		if (data->type == 0) {
			ada_free_search_params_keys_iter(data->it.keys);
		} else if (data->type == 1) {
			ada_free_search_params_values_iter(data->it.values);
		} else if (data->type == 2) {
			ada_free_search_params_entries_iter(data->it.entries);
		}
		js_free_rt(rt, data);
	}
}

static JSValue nx_url_can_parse(JSContext *ctx, JSValueConst this_val, int argc,
								JSValueConst *argv) {
	STR(input, 0);
	ada_url url;
	if (argc == 2 && !JS_IsUndefined(argv[1])) {
		STR(base, 1);
		url = ada_parse_with_base(input, input_length, base, base_length);
		JS_FreeCString(ctx, input);
		input = base;
	} else {
		url = ada_parse(input, input_length);
	}
	JSValue rtn = JS_NewBool(ctx, ada_is_valid(url));
	ada_free(url);
	JS_FreeCString(ctx, input);
	return rtn;
}

static JSValue nx_url_new(JSContext *ctx, JSValueConst this_val, int argc,
						  JSValueConst *argv) {
	STR(input, 0);
	ada_url url;
	if (argc == 2 && !JS_IsUndefined(argv[1])) {
		STR(base, 1);
		url = ada_parse_with_base(input, input_length, base, base_length);
		JS_FreeCString(ctx, input);
		input = base;
	} else {
		url = ada_parse(input, input_length);
	}
	if (!ada_is_valid(url)) {
		JS_ThrowTypeError(ctx, "%s is not a valid URL", input);
		JS_FreeCString(ctx, input);
		return JS_EXCEPTION;
	}
	JS_FreeCString(ctx, input);
	nx_url_t *data = js_mallocz(ctx, sizeof(nx_url_t));
	if (!data) {
		return JS_EXCEPTION;
	}
	data->url = url;
	JSValue url_obj = JS_NewObjectClass(ctx, nx_url_class_id);
	JS_SetOpaque(url_obj, data);
	return url_obj;
}

#define DEFINE_GETTER_SETTER(NAME)                                             \
	static JSValue nx_url_get_##NAME(JSContext *ctx, JSValueConst this_val,    \
									 int argc, JSValueConst *argv) {           \
		nx_url_t *data = JS_GetOpaque2(ctx, this_val, nx_url_class_id);        \
		if (!data) {                                                           \
			return JS_EXCEPTION;                                               \
		}                                                                      \
		ada_string val = ada_get_##NAME(data->url);                            \
		return JS_NewStringLen(ctx, val.data, val.length);                     \
	}                                                                          \
	static JSValue nx_url_set_##NAME(JSContext *ctx, JSValueConst this_val,    \
									 int argc, JSValueConst *argv) {           \
		STR(val, 0);                                                           \
		nx_url_t *data = JS_GetOpaque2(ctx, this_val, nx_url_class_id);        \
		if (!data) {                                                           \
			return JS_EXCEPTION;                                               \
		}                                                                      \
		ada_set_##NAME(data->url, val, val_length);                            \
		JS_FreeCString(ctx, val);                                              \
		return JS_UNDEFINED;                                                   \
	}

DEFINE_GETTER_SETTER(hash)
DEFINE_GETTER_SETTER(host)
DEFINE_GETTER_SETTER(hostname)
DEFINE_GETTER_SETTER(password)
DEFINE_GETTER_SETTER(pathname)
DEFINE_GETTER_SETTER(port)
DEFINE_GETTER_SETTER(protocol)
DEFINE_GETTER_SETTER(username)

static JSValue nx_url_get_search(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	nx_url_t *data = JS_GetOpaque2(ctx, this_val, nx_url_class_id);
	if (!data) {
		return JS_EXCEPTION;
	}
	if (data->params && data->params_modified) {
		ada_owned_string val = ada_search_params_to_string(data->params);
		JSValue str;
		if (val.length == 0) {
			str = JS_NewString(ctx, "");
		} else {
			char val_with_question_mark[val.length + 1];
			val_with_question_mark[0] = '?';
			memcpy(val_with_question_mark + 1, val.data, val.length);
			str = JS_NewStringLen(ctx, val_with_question_mark, val.length + 1);
		}
		ada_free_owned_string(val);
		return str;
	} else {
		ada_string val = ada_get_search(data->url);
		return JS_NewStringLen(ctx, val.data, val.length);
	}
}

static JSValue nx_url_set_search(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	STR(val, 0);
	nx_url_t *data = JS_GetOpaque2(ctx, this_val, nx_url_class_id);
	if (!data) {
		return JS_EXCEPTION;
	}
	if (data->params) {
		ada_free_search_params(data->params);
		data->params = ada_parse_search_params(val, val_length);
	}
	ada_set_search(data->url, val, val_length);
	data->params_modified = false;
	JS_FreeCString(ctx, val);
	return JS_UNDEFINED;
}

static JSValue nx_url_get_href(JSContext *ctx, JSValueConst this_val, int argc,
							   JSValueConst *argv) {
	nx_url_t *data = JS_GetOpaque2(ctx, this_val, nx_url_class_id);
	if (!data) {
		return JS_EXCEPTION;
	}
	if (data->params) {
		ada_owned_string val = ada_search_params_to_string(data->params);
		ada_set_search(data->url, val.data, val.length);
		ada_free_owned_string(val);
	}
	ada_string val = ada_get_href(data->url);
	return JS_NewStringLen(ctx, val.data, val.length);
}

static JSValue nx_url_set_href(JSContext *ctx, JSValueConst this_val, int argc,
							   JSValueConst *argv) {
	STR(val, 0);
	nx_url_t *data = JS_GetOpaque2(ctx, this_val, nx_url_class_id);
	if (!data) {
		return JS_EXCEPTION;
	}
	ada_set_href(data->url, val, val_length);
	JS_FreeCString(ctx, val);
	if (data->params) {
		ada_free_search_params(data->params);
		ada_string search_val = ada_get_search(data->url);
		data->params =
			ada_parse_search_params(search_val.data, search_val.length);
	}
	data->params_modified = false;
	return JS_UNDEFINED;
}

static JSValue nx_url_get_origin(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	nx_url_t *data = JS_GetOpaque2(ctx, this_val, nx_url_class_id);
	if (!data) {
		return JS_EXCEPTION;
	}
	ada_owned_string val = ada_get_origin(data->url);
	JSValue str = JS_NewStringLen(ctx, val.data, val.length);
	ada_free_owned_string(val);
	return str;
}

static JSValue nx_url_init(JSContext *ctx, JSValueConst this_val, int argc,
						   JSValueConst *argv) {
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
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
	JS_FreeValue(ctx, proto);

	// Static method
	NX_DEF_FUNC(argv[0], "canParse", nx_url_can_parse, 1);

	return JS_UNDEFINED;
}

static JSValue nx_url_search_params_new(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	nx_url_t *data;
	if (argc == 2 && !JS_IsUndefined(argv[1])) {
		// Accessing `searchParams` on a `URL` instance
		data = JS_GetOpaque2(ctx, argv[1], nx_url_class_id);
	} else {
		data = js_mallocz(ctx, sizeof(nx_url_t));
	}
	if (!data) {
		return JS_EXCEPTION;
	}

	STR(input, 0);
	data->params = ada_parse_search_params(input, input_length);
	JS_FreeCString(ctx, input);

	JSValue params_obj = JS_NewObjectClass(ctx, nx_url_search_params_class_id);
	JS_SetOpaque(params_obj, data);

	return params_obj;
}

static JSValue nx_url_search_params_get_size(JSContext *ctx,
											 JSValueConst this_val, int argc,
											 JSValueConst *argv) {
	nx_url_t *data =
		JS_GetOpaque2(ctx, this_val, nx_url_search_params_class_id);
	if (!data) {
		return JS_EXCEPTION;
	}
	size_t size = ada_search_params_size(data->params);
	return JS_NewUint32(ctx, size);
}

static JSValue nx_url_search_params_append(JSContext *ctx,
										   JSValueConst this_val, int argc,
										   JSValueConst *argv) {
	nx_url_t *data =
		JS_GetOpaque2(ctx, this_val, nx_url_search_params_class_id);
	if (!data) {
		return JS_EXCEPTION;
	}
	STR(key, 0);
	STR(value, 1);
	ada_search_params_append(data->params, key, key_length, value,
							 value_length);
	data->params_modified = true;
	JS_FreeCString(ctx, key);
	JS_FreeCString(ctx, value);
	return JS_UNDEFINED;
}

static JSValue nx_url_search_params_delete(JSContext *ctx,
										   JSValueConst this_val, int argc,
										   JSValueConst *argv) {
	nx_url_t *data =
		JS_GetOpaque2(ctx, this_val, nx_url_search_params_class_id);
	if (!data) {
		return JS_EXCEPTION;
	}
	STR(key, 0);
	if (argc == 2 && JS_IsString(argv[1])) {
		STR(value, 1);
		ada_search_params_remove_value(data->params, key, key_length, value,
									   value_length);
		JS_FreeCString(ctx, value);
	} else {
		ada_search_params_remove(data->params, key, key_length);
	}
	data->params_modified = true;
	JS_FreeCString(ctx, key);
	return JS_UNDEFINED;
}

static JSValue nx_url_search_params_get(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	nx_url_t *data =
		JS_GetOpaque2(ctx, this_val, nx_url_search_params_class_id);
	if (!data) {
		return JS_EXCEPTION;
	}
	STR(key, 0);
	ada_string val = ada_search_params_get(data->params, key, key_length);
	JS_FreeCString(ctx, key);
	return JS_NewStringLen(ctx, val.data, val.length);
}

static JSValue nx_url_search_params_get_all(JSContext *ctx,
											JSValueConst this_val, int argc,
											JSValueConst *argv) {
	nx_url_t *data =
		JS_GetOpaque2(ctx, this_val, nx_url_search_params_class_id);
	if (!data) {
		return JS_EXCEPTION;
	}
	STR(key, 0);
	ada_strings vals = ada_search_params_get_all(data->params, key, key_length);
	JS_FreeCString(ctx, key);
	JSValue arr = JS_NewArray(ctx);
	size_t len = ada_strings_size(vals);
	for (size_t i = 0; i < len; i++) {
		ada_string val = ada_strings_get(vals, i);
		JS_SetPropertyUint32(ctx, arr, i,
							 JS_NewStringLen(ctx, val.data, val.length));
	}
	ada_free_strings(vals);
	return arr;
}

static JSValue nx_url_search_params_has(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	nx_url_t *data =
		JS_GetOpaque2(ctx, this_val, nx_url_search_params_class_id);
	if (!data) {
		return JS_EXCEPTION;
	}
	STR(key, 0);
	bool has = ada_search_params_has(data->params, key, key_length);
	JS_FreeCString(ctx, key);
	return JS_NewBool(ctx, has);
}

static JSValue nx_url_search_params_set(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	nx_url_t *data =
		JS_GetOpaque2(ctx, this_val, nx_url_search_params_class_id);
	if (!data) {
		return JS_EXCEPTION;
	}
	STR(key, 0);
	STR(value, 1);
	ada_search_params_set(data->params, key, key_length, value, value_length);
	data->params_modified = true;
	JS_FreeCString(ctx, key);
	JS_FreeCString(ctx, value);
	return JS_UNDEFINED;
}

static JSValue nx_url_search_params_sort(JSContext *ctx, JSValueConst this_val,
										 int argc, JSValueConst *argv) {
	nx_url_t *data =
		JS_GetOpaque2(ctx, this_val, nx_url_search_params_class_id);
	if (!data) {
		return JS_EXCEPTION;
	}
	ada_search_params_sort(data->params);
	data->params_modified = true;
	return JS_UNDEFINED;
}

static JSValue nx_url_search_params_to_string(JSContext *ctx,
											  JSValueConst this_val, int argc,
											  JSValueConst *argv) {
	nx_url_t *data =
		JS_GetOpaque2(ctx, this_val, nx_url_search_params_class_id);
	if (!data) {
		return JS_EXCEPTION;
	}
	ada_owned_string val = ada_search_params_to_string(data->params);
	JSValue str = JS_NewStringLen(ctx, val.data, val.length);
	ada_free_owned_string(val);
	return str;
}

static JSValue nx_url_search_params_init(JSContext *ctx, JSValueConst this_val,
										 int argc, JSValueConst *argv) {
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_GET(proto, "size", nx_url_search_params_get_size);
	NX_DEF_FUNC(proto, "append", nx_url_search_params_append, 2);
	NX_DEF_FUNC(proto, "delete", nx_url_search_params_delete, 1);
	NX_DEF_FUNC(proto, "get", nx_url_search_params_get, 1);
	NX_DEF_FUNC(proto, "getAll", nx_url_search_params_get_all, 1);
	NX_DEF_FUNC(proto, "has", nx_url_search_params_has, 1);
	NX_DEF_FUNC(proto, "set", nx_url_search_params_set, 2);
	NX_DEF_FUNC(proto, "sort", nx_url_search_params_sort, 0);
	NX_DEF_FUNC(proto, "toString", nx_url_search_params_to_string, 0);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static JSValue nx_url_search_params_iterator(JSContext *ctx,
											 JSValueConst this_val, int argc,
											 JSValueConst *argv) {
	nx_url_t *params_data =
		JS_GetOpaque2(ctx, argv[0], nx_url_search_params_class_id);
	if (!params_data) {
		return JS_EXCEPTION;
	}
	u32 type;
	if (JS_ToUint32(ctx, &type, argv[1])) {
		return JS_EXCEPTION;
	}
	nx_url_search_params_iterator_t *data =
		js_mallocz(ctx, sizeof(nx_url_search_params_iterator_t));
	if (!data) {
		return JS_EXCEPTION;
	}
	data->type = type;
	if (type == 0) {
		data->it.keys = ada_search_params_get_keys(params_data->params);
	} else if (type == 1) {
		data->it.values = ada_search_params_get_values(params_data->params);
	} else if (type == 2) {
		data->it.entries = ada_search_params_get_entries(params_data->params);
	} else {
		js_free(ctx, data);
		return JS_ThrowTypeError(
			ctx, "Invalid URLSearchParams iterator type %d", type);
	}
	JSValue obj =
		JS_NewObjectClass(ctx, nx_url_search_params_iterator_class_id);
	JS_SetOpaque(obj, data);
	return obj;
}

static JSValue nx_url_search_params_iterator_next(JSContext *ctx,
												  JSValueConst this_val,
												  int argc,
												  JSValueConst *argv) {
	nx_url_search_params_iterator_t *data =
		JS_GetOpaque2(ctx, argv[0], nx_url_search_params_iterator_class_id);
	if (!data) {
		return JS_EXCEPTION;
	}
	if (data->type == 0) {
		if (ada_search_params_keys_iter_has_next(data->it.keys)) {
			ada_string val = ada_search_params_keys_iter_next(data->it.keys);
			return JS_NewStringLen(ctx, val.data, val.length);
		}
	} else if (data->type == 1) {
		if (ada_search_params_values_iter_has_next(data->it.values)) {
			ada_string val =
				ada_search_params_values_iter_next(data->it.values);
			return JS_NewStringLen(ctx, val.data, val.length);
		}
	} else if (data->type == 2) {
		if (ada_search_params_entries_iter_has_next(data->it.entries)) {
			ada_string_pair pair =
				ada_search_params_entries_iter_next(data->it.entries);
			JSValue arr = JS_NewArray(ctx);
			JS_SetPropertyUint32(
				ctx, arr, 0,
				JS_NewStringLen(ctx, pair.key.data, pair.key.length));
			JS_SetPropertyUint32(
				ctx, arr, 1,
				JS_NewStringLen(ctx, pair.value.data, pair.value.length));
			return arr;
		}
	}
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("urlNew", 1, nx_url_new),
	JS_CFUNC_DEF("urlInit", 1, nx_url_init),
	JS_CFUNC_DEF("urlSearchNew", 1, nx_url_search_params_new),
	JS_CFUNC_DEF("urlSearchInit", 1, nx_url_search_params_init),
	JS_CFUNC_DEF("urlSearchIterator", 2, nx_url_search_params_iterator),
	JS_CFUNC_DEF("urlSearchIteratorNext", 1,
				 nx_url_search_params_iterator_next),
};

void nx_init_url(JSContext *ctx, JSValueConst init_obj) {
	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_url_class_id);
	JSClassDef url_class = {
		"URL",
		.finalizer = finalizer_url,
	};
	JS_NewClass(rt, nx_url_class_id, &url_class);

	JS_NewClassID(rt, &nx_url_search_params_class_id);
	JSClassDef url_search_params_class = {
		"URLSearchParams",
		.finalizer = finalizer_url_search_params,
	};
	JS_NewClass(rt, nx_url_search_params_class_id, &url_search_params_class);

	JS_NewClassID(rt, &nx_url_search_params_iterator_class_id);
	JSClassDef url_search_params_iterator_class = {
		"URLSearchParams Iterator",
		.finalizer = finalizer_url_search_params_iterator,
	};
	JS_NewClass(rt, nx_url_search_params_iterator_class_id,
				&url_search_params_iterator_class);

	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
