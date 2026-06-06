#include "error.h"
#include "types.h"
#include "util.h"
#include "wrap.h"

using namespace v8;

namespace {

typedef struct {
	Service service;
} nx_service_t;

void free_service(nx_service_t *data) {
	serviceClose(&data->service);
	free(data);
}

nx_service_t *get_service(Local<Value> v) {
	return nx::Unwrap<nx_service_t>(v);
}

void nx_service_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_service_t *data = (nx_service_t *)calloc(1, sizeof(nx_service_t));
	if (info[0]->IsString()) {
		String::Utf8Value name(iso, info[0]);
		Result rc = smGetService(&data->service, *name ? *name : "");
		if (R_FAILED(rc)) {
			free(data);
			nx_throw_libnx_error(iso, rc, "smGetService");
			return;
		}
	}
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_service_t>(iso, obj, data, free_service);
	info.GetReturnValue().Set(obj);
}

void nx_service_is_active(const FunctionCallbackInfo<Value> &info) {
	nx_service_t *data = get_service(info.This());
	if (data)
		info.GetReturnValue().Set(
		    Boolean::New(info.GetIsolate(), serviceIsActive(&data->service)));
}
void nx_service_is_domain(const FunctionCallbackInfo<Value> &info) {
	nx_service_t *data = get_service(info.This());
	if (data)
		info.GetReturnValue().Set(
		    Boolean::New(info.GetIsolate(), serviceIsDomain(&data->service)));
}
void nx_service_is_override(const FunctionCallbackInfo<Value> &info) {
	nx_service_t *data = get_service(info.This());
	if (data)
		info.GetReturnValue().Set(
		    Boolean::New(info.GetIsolate(), serviceIsOverride(&data->service)));
}

// Helpers to read typed properties off an object.
uint32_t prop_u32(Isolate *iso, Local<Object> o, const char *k, uint32_t def) {
	Local<Context> c = iso->GetCurrentContext();
	Local<Value> v;
	if (o->Get(c, nx_str(iso, k)).ToLocal(&v) && v->IsNumber()) {
		uint32_t n;
		if (v->Uint32Value(c).To(&n))
			return n;
	}
	return def;
}

void nx_service_dispatch_in_out(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> c = iso->GetCurrentContext();
	nx_service_t *data = get_service(info.This());
	if (!data)
		return;
	uint32_t rid;
	if (!info[0]->Uint32Value(c).To(&rid))
		return;
	size_t in_data_size = 0, out_data_size = 0;
	void *in_data = NX_GetBufferSource(iso, &in_data_size, info[1]);
	void *out_data = NX_GetBufferSource(iso, &out_data_size, info[2]);

	SfDispatchParams disp = {};
	if (info[3]->IsObject()) {
		Local<Object> opts = info[3].As<Object>();
		Local<Value> v;
		if (opts->Get(c, nx_str(iso, "targetSession")).ToLocal(&v) &&
		    v->IsNumber()) {
			uint32_t ts = 0;
			if (v->Uint32Value(c).To(&ts))
				disp.target_session = ts;
		}
		disp.context = prop_u32(iso, opts, "context", 0);

		// bufferAttrs (array of up to 8 numbers)
		if (opts->Get(c, nx_str(iso, "bufferAttrs")).ToLocal(&v) &&
		    v->IsArray()) {
			Local<Object> a = v.As<Object>();
			u32 *attrs = &disp.buffer_attrs.attr0;
			for (int i = 0; i < 8; i++) {
				Local<Value> e;
				if (a->Get(c, i).ToLocal(&e) && e->IsNumber()) {
					uint32_t n;
					if (e->Uint32Value(c).To(&n))
						attrs[i] = n;
				}
			}
		}
		// buffers (array of BufferSource)
		if (opts->Get(c, nx_str(iso, "buffers")).ToLocal(&v) && v->IsArray()) {
			Local<Object> a = v.As<Object>();
			uint32_t length = 0;
			Local<Value> len_v;
			if (!a->Get(c, nx_str(iso, "length")).ToLocal(&len_v))
				return;
			if (!len_v->Uint32Value(c).To(&length))
				length = 0;
			for (uint32_t i = 0; i < length && i < 8; i++) {
				Local<Value> e;
				if (a->Get(c, i).ToLocal(&e)) {
					disp.buffers[i].ptr =
					    NX_GetBufferSource(iso, &disp.buffers[i].size, e);
				}
			}
		}
		if (opts->Get(c, nx_str(iso, "inSendPid")).ToLocal(&v))
			disp.in_send_pid = v->BooleanValue(iso);
		// inObjects
		if (opts->Get(c, nx_str(iso, "inObjects")).ToLocal(&v) &&
		    v->IsArray()) {
			Local<Object> a = v.As<Object>();
			Local<Value> len_v;
			if (!a->Get(c, nx_str(iso, "length")).ToLocal(&len_v))
				return;
			if (!len_v->Uint32Value(c).To(&disp.in_num_objects))
				disp.in_num_objects = 0;
			for (uint32_t i = 0; i < disp.in_num_objects && i < 8; i++) {
				Local<Value> e;
				if (!a->Get(c, i).ToLocal(&e))
					return;
				nx_service_t *vd = get_service(e);
				if (!vd) {
					nx_throw(iso, "invalid inObjects entry");
					return;
				}
				disp.in_objects[i] = &vd->service;
			}
		}
		// outObjects
		if (opts->Get(c, nx_str(iso, "outObjects")).ToLocal(&v) &&
		    v->IsArray()) {
			Local<Object> a = v.As<Object>();
			Local<Value> len_v;
			if (!a->Get(c, nx_str(iso, "length")).ToLocal(&len_v))
				return;
			if (!len_v->Uint32Value(c).To(&disp.out_num_objects))
				disp.out_num_objects = 0;
			for (uint32_t i = 0; i < disp.out_num_objects && i < 8; i++) {
				Local<Value> e;
				if (!a->Get(c, i).ToLocal(&e))
					return;
				nx_service_t *vd = get_service(e);
				if (!vd) {
					nx_throw(iso, "invalid outObjects entry");
					return;
				}
				disp.out_objects = &vd->service;
			}
		}
	}
	Result rc = serviceDispatchImpl(&data->service, rid, in_data, in_data_size,
	                                out_data, out_data_size, disp);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "serviceDispatchOut");
	}
}

void nx_service_init(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> c = iso->GetCurrentContext();
	Local<Object> proto = info[0]
	                          .As<Object>()
	                          ->Get(c, nx_str(iso, "prototype"))
	                          .ToLocalChecked()
	                          .As<Object>();
	NX_DEF_FUNC(proto, "isActive", nx_service_is_active, 0);
	NX_DEF_FUNC(proto, "isDomain", nx_service_is_domain, 0);
	NX_DEF_FUNC(proto, "isOverride", nx_service_is_override, 0);
	NX_DEF_FUNC(proto, "dispatchInOut", nx_service_dispatch_in_out, 3);
}

} // namespace

void nx_init_service(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "serviceInit", nx_service_init);
	NX_SET_FUNC(init_obj, "serviceNew", nx_service_new);
}
