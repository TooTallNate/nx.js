#include "service.h"
#include "error.h"
#include "util.h"

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

	// If a string is passed in, then it is the name of the service module
	if (JS_IsString(argv[0])) {
		const char *name = JS_ToCString(ctx, argv[0]);
		if (!name) {
			return JS_EXCEPTION;
		}

		Result rc = smGetService(&data->service, name);
		JS_FreeCString(ctx, name);

		if (R_FAILED(rc)) {
			return nx_throw_libnx_error(ctx, rc, "smGetService()");
		}
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

static JSValue nx_service_is_domain(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_service_t *data = JS_GetOpaque2(ctx, this_val, nx_service_class_id);
	if (!data)
		return JS_EXCEPTION;

	return JS_NewBool(ctx, serviceIsDomain(&data->service));
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
	void *in_data = NX_GetArrayBufferView(ctx, &in_data_size, argv[1]);

	size_t out_data_size = 0;
	void *out_data = NX_GetArrayBufferView(ctx, &out_data_size, argv[2]);

	SfDispatchParams disp = {0};
	if (JS_IsObject(argv[3])) {
		// disp.target_session
		JSValue target_session_val =
			JS_GetPropertyStr(ctx, argv[3], "targetSession");
		if (JS_IsNumber(target_session_val)) {
			if (JS_ToUint32(ctx, &disp.target_session, target_session_val)) {
				JS_FreeValue(ctx, target_session_val);
				return JS_EXCEPTION;
			}
		}
		JS_FreeValue(ctx, target_session_val);

		// disp.context
		JSValue context_val = JS_GetPropertyStr(ctx, argv[3], "context");
		if (JS_IsNumber(context_val)) {
			if (JS_ToUint32(ctx, &disp.context, context_val)) {
				JS_FreeValue(ctx, context_val);
				return JS_EXCEPTION;
			}
		}
		JS_FreeValue(ctx, context_val);

		// disp.buffer_attrs
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
			u32 attr;
			JSValue buffer_attr_val;

#define GET_BUFFER_ATTR(INDEX)                                                 \
	buffer_attr_val = JS_GetPropertyStr(ctx, buffer_attrs_val, #INDEX);        \
	if (JS_IsNumber(buffer_attr_val)) {                                        \
		if (JS_ToUint32(ctx, &attr, buffer_attr_val)) {                        \
			JS_FreeValue(ctx, buffer_attr_val);                                \
			JS_FreeValue(ctx, buffer_attrs_val);                               \
			JS_FreeValue(ctx, length_val);                                     \
			return JS_EXCEPTION;                                               \
		}                                                                      \
		disp.buffer_attrs.attr##INDEX = attr;                                  \
	}                                                                          \
	JS_FreeValue(ctx, buffer_attr_val);

			GET_BUFFER_ATTR(0);
			GET_BUFFER_ATTR(1);
			GET_BUFFER_ATTR(2);
			GET_BUFFER_ATTR(3);
			GET_BUFFER_ATTR(4);
			GET_BUFFER_ATTR(5);
			GET_BUFFER_ATTR(6);
			GET_BUFFER_ATTR(7);

#undef GET_BUFFER_ATTR

			JS_FreeValue(ctx, length_val);
		}
		JS_FreeValue(ctx, buffer_attrs_val);

		// disp.buffers
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
				disp.buffers[i].ptr =
					NX_GetArrayBufferView(ctx, &disp.buffers[i].size, v);
				JS_FreeValue(ctx, v);
			}
		}
		JS_FreeValue(ctx, buffers_val);

		// disp.in_send_pid
		JSValue in_send_pid_val = JS_GetPropertyStr(ctx, argv[3], "inSendPid");
		if (JS_IsBool(in_send_pid_val)) {
			disp.in_send_pid = JS_ToBool(ctx, in_send_pid_val);
		}
		JS_FreeValue(ctx, in_send_pid_val);

		// disp.in_num_objects
		// disp.in_objects
		JSValue in_objects_val = JS_GetPropertyStr(ctx, argv[3], "inObjects");
		if (JS_IsArray(ctx, in_objects_val)) {
			JSValue length_val =
				JS_GetPropertyStr(ctx, in_objects_val, "length");
			if (JS_ToUint32(ctx, &disp.in_num_objects, length_val)) {
				JS_FreeValue(ctx, in_objects_val);
				JS_FreeValue(ctx, length_val);
				return JS_EXCEPTION;
			}

			for (u32 i = 0; i < disp.in_num_objects; i++) {
				JSValue v = JS_GetPropertyUint32(ctx, in_objects_val, i);
				nx_service_t *v_data =
					JS_GetOpaque2(ctx, v, nx_service_class_id);
				if (!v_data) {
					JS_FreeValue(ctx, v);
					JS_FreeValue(ctx, in_objects_val);
					JS_FreeValue(ctx, length_val);
					return JS_EXCEPTION;
				}
				disp.in_objects[i] = &v_data->service;
				JS_FreeValue(ctx, v);
			}

			JS_FreeValue(ctx, length_val);
		}
		JS_FreeValue(ctx, in_objects_val);

		// disp.out_num_objects
		// disp.out_objects
		JSValue out_objects_val = JS_GetPropertyStr(ctx, argv[3], "outObjects");
		if (JS_IsArray(ctx, out_objects_val)) {
			JSValue length_val =
				JS_GetPropertyStr(ctx, out_objects_val, "length");
			if (JS_ToUint32(ctx, &disp.out_num_objects, length_val)) {
				JS_FreeValue(ctx, out_objects_val);
				JS_FreeValue(ctx, length_val);
				return JS_EXCEPTION;
			}

			for (u32 i = 0; i < disp.out_num_objects; i++) {
				JSValue v = JS_GetPropertyUint32(ctx, out_objects_val, i);
				nx_service_t *v_data =
					JS_GetOpaque2(ctx, v, nx_service_class_id);
				if (!v_data) {
					JS_FreeValue(ctx, v);
					JS_FreeValue(ctx, out_objects_val);
					JS_FreeValue(ctx, length_val);
					return JS_EXCEPTION;
				}
				// XXX: This seems to always be 1 in libnx (hence why its not an
				// array?)
				disp.out_objects = &v_data->service;
				JS_FreeValue(ctx, v);
			}

			JS_FreeValue(ctx, length_val);
		}
		JS_FreeValue(ctx, out_objects_val);
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
	NX_DEF_FUNC(proto, "isDomain", nx_service_is_domain, 0);
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
