#include "service.h"
#include "error.h"

static JSClassID nx_service_class_id;

typedef struct {
	Service service;
} nx_service_t;

static void finalizer_service(JSRuntime *rt, JSValue val) {
	nx_service_t *data = JS_GetOpaque(val, nx_service_class_id);
	if (data) {
		serviceClose(&data->service);
		js_free_rt(rt, data);
	}
}

static JSValue nx_service_new(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	nx_service_t *data = js_mallocz(ctx, sizeof(nx_service_t));
	if (!data) {
		return JS_EXCEPTION;
	}

	const char *name = JS_ToCString(ctx, argv[0]);
	if (!name) {
		return JS_EXCEPTION;
	}

	Result rc = smGetService(&data->service, name);
	JS_FreeCString(ctx, name);

	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "smGetService()");
	}

	JSValue obj = JS_NewObjectClass(ctx, nx_service_class_id);
	JS_SetOpaque(obj, data);

	return obj;
}

static JSValue nx_service_is_active(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_service_t *data = JS_GetOpaque2(ctx, this_val, nx_service_class_id);
	if (!data)
		return JS_EXCEPTION;

	return JS_NewBool(ctx, serviceIsActive(&data->service));
}

static JSValue nx_service_is_override(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_service_t *data = JS_GetOpaque2(ctx, this_val, nx_service_class_id);
	if (!data)
		return JS_EXCEPTION;

	return JS_NewBool(ctx, serviceIsOverride(&data->service));
}

static JSValue nx_service_dispatch_in_out(JSContext *ctx, JSValueConst this_val,
										  int argc, JSValueConst *argv) {
	nx_service_t *data = JS_GetOpaque2(ctx, this_val, nx_service_class_id);
	if (!data)
		return JS_EXCEPTION;

	u32 rid;
	if (JS_ToUint32(ctx, &rid, argv[0])) {
		return JS_EXCEPTION;
	}

	size_t in_data_size = 0;
	void *in_data = NULL;
	if (JS_IsArrayBuffer(argv[1])) {
		in_data = JS_GetArrayBuffer(ctx, &in_data_size, argv[1]);
	}

	size_t out_data_size = 0;
	void *out_data = NULL;
	if (JS_IsArrayBuffer(argv[2])) {
		out_data = JS_GetArrayBuffer(ctx, &out_data_size, argv[2]);
	}

	SfDispatchParams disp = {0};
	if (JS_IsObject(argv[3])) {
		JSValue buffer_attrs_val =
			JS_GetPropertyStr(ctx, argv[3], "bufferAttrs");
		if (JS_IsArray(ctx, buffer_attrs_val)) {
			JSValue length_val =
				JS_GetPropertyStr(ctx, buffer_attrs_val, "length");
			u32 length;
			if (JS_ToUint32(ctx, &length, length_val)) {
				JS_FreeValue(ctx, buffer_attrs_val);
				JS_FreeValue(ctx, length_val);
				return JS_EXCEPTION;
			}
			for (u32 i = 0; i < length; i++) {
				JSValue v = JS_GetPropertyUint32(ctx, buffer_attrs_val, i);
				if (JS_IsNumber(v)) {
					u32 attr;
					if (JS_ToUint32(ctx, &attr, v)) {
						JS_FreeValue(ctx, buffer_attrs_val);
						JS_FreeValue(ctx, length_val);
						return JS_EXCEPTION;
					}
					// TODO: all `attr` props
					disp.buffer_attrs.attr0 = attr;
				}
				JS_FreeValue(ctx, v);
			}
			JS_FreeValue(ctx, length_val);
		}
		JS_FreeValue(ctx, buffer_attrs_val);

		JSValue buffers_val = JS_GetPropertyStr(ctx, argv[3], "buffers");
		if (JS_IsArray(ctx, buffers_val)) {
			JSValue length_val = JS_GetPropertyStr(ctx, buffers_val, "length");
			u32 length;
			if (JS_ToUint32(ctx, &length, length_val)) {
				JS_FreeValue(ctx, buffers_val);
				JS_FreeValue(ctx, length_val);
				return JS_EXCEPTION;
			}
			for (u32 i = 0; i < length; i++) {
				JSValue v = JS_GetPropertyUint32(ctx, buffers_val, i);
				if (JS_IsArrayBuffer(v)) {
					disp.buffers[i].ptr =
						JS_GetArrayBuffer(ctx, &disp.buffers[i].size, v);
				}
				JS_FreeValue(ctx, v);
			}
		}
		JS_FreeValue(ctx, buffers_val);
	}

	Result rc = serviceDispatchImpl(&data->service, rid, in_data, in_data_size,
									out_data, out_data_size, disp);

	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "serviceDispatchOut()");
	}

	return JS_UNDEFINED;
}

static JSValue nx_service_init(JSContext *ctx, JSValueConst this_val, int argc,
							   JSValueConst *argv) {
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_FUNC(proto, "isActive", nx_service_is_active, 0);
	NX_DEF_FUNC(proto, "isOverride", nx_service_is_override, 0);
	NX_DEF_FUNC(proto, "dispatchInOut", nx_service_dispatch_in_out, 3);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("serviceInit", 3, nx_service_init),
	JS_CFUNC_DEF("serviceNew", 3, nx_service_new),
};

void nx_init_service(JSContext *ctx, JSValueConst init_obj) {
	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_service_class_id);
	JSClassDef nx_service_class = {
		"Service",
		.finalizer = finalizer_service,
	};
	JS_NewClass(rt, nx_service_class_id, &nx_service_class);

	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
