#include "error.h"
#include "image.h"
#include "types.h"
#include "wrap.h"
#include <cairo.h>

using namespace v8;

namespace {

// BGRA8(r,g,b,a) is provided as a macro by the libnx/switch headers.

struct RGBA {
	u8 r, g, b, a;
};

typedef struct {
	IrsIrCameraHandle irhandle;
	IrsImageTransferProcessorConfig config;
	struct RGBA color;
	nx_image_t *image;
	u8 *sensor_buf;
	u32 sensor_buf_size;
	u64 sampling_number;
} nx_ir_sensor_t;

void free_ir_sensor(nx_ir_sensor_t *data) {
	if (data->sensor_buf)
		free(data->sensor_buf);
	irsStopImageProcessor(data->irhandle);
	free(data);
}

nx_ir_sensor_t *get_sensor(Local<Value> v) {
	return nx::Unwrap<nx_ir_sensor_t>(v);
}

void nx_irs_exit(const FunctionCallbackInfo<Value> &info) { irsExit(); }

void nx_irs_initialize(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Result rc = irsInitialize();
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "irsInitialize");
		return;
	}
	Local<Context> context = iso->GetCurrentContext();
	info.GetReturnValue().Set(FunctionTemplate::New(iso, nx_irs_exit)
	                              ->GetFunction(context)
	                              .ToLocalChecked());
}

void nx_irs_sensor_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	nx_ir_sensor_t *data =
	    (nx_ir_sensor_t *)calloc(1, sizeof(nx_ir_sensor_t));
	data->sampling_number = (u64)-1;
	data->image = nx_get_image(iso, info[0]);
	if (!data->image) {
		free(data);
		nx_throw(iso, "invalid image");
		return;
	}
	data->sensor_buf_size = data->image->width * data->image->height;
	data->sensor_buf = (u8 *)calloc(1, data->sensor_buf_size);

	Local<Object> colors = info[1].As<Object>();
	uint32_t r = 0, g = 0, b = 0;
	double a = 0;
	if (!colors->Get(context, 0).ToLocalChecked()->Uint32Value(context).To(&r))
		r = 0;
	if (!colors->Get(context, 1).ToLocalChecked()->Uint32Value(context).To(&g))
		g = 0;
	if (!colors->Get(context, 2).ToLocalChecked()->Uint32Value(context).To(&b))
		b = 0;
	if (!colors->Get(context, 3).ToLocalChecked()->NumberValue(context).To(&a))
		a = 0;
	data->color.r = r;
	data->color.g = g;
	data->color.b = b;
	data->color.a = (u8)(a * 255);

	HidNpadIdType id = HidNpadIdType_Handheld;
	Result rc = irsGetIrCameraHandle(&data->irhandle, id);
	if (R_FAILED(rc)) {
		free(data->sensor_buf);
		free(data);
		nx_throw_libnx_error(iso, rc, "irsGetIrCameraHandle");
		return;
	}
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_ir_sensor_t>(iso, obj, data, free_ir_sensor);
	info.GetReturnValue().Set(obj);
}

void nx_irs_sensor_start(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_ir_sensor_t *data = get_sensor(info[0]);
	if (!data)
		return;
	irsGetDefaultImageTransferProcessorConfig(&data->config);
	Result rc =
	    irsRunImageTransferProcessor(data->irhandle, &data->config, 0x100000);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "irsRunImageTransferProcessor");
	}
}

void nx_irs_sensor_stop(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_ir_sensor_t *data = get_sensor(info[0]);
	if (!data)
		return;
	Result rc = irsStopImageProcessor(data->irhandle);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "irsStopImageProcessor");
	}
}

void nx_irs_sensor_update(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_ir_sensor_t *data = get_sensor(info[0]);
	if (!data)
		return;
	IrsImageTransferProcessorState state;
	Result rc = irsGetImageTransferProcessorState(
	    data->irhandle, data->sensor_buf, data->sensor_buf_size, &state);
	bool updated = false;
	if (R_SUCCEEDED(rc) && state.sampling_number != data->sampling_number) {
		for (u32 y = 0; y < data->image->height; y++) {
			for (u32 x = 0; x < data->image->width; x++) {
				u32 pos = y * data->image->width + x;
				u8 alpha = (data->sensor_buf[pos] * data->color.a) / 255;
				((u32 *)data->image->data)[pos] =
				    BGRA8((data->color.r * alpha) / 255,
				          (data->color.g * alpha) / 255,
				          (data->color.b * alpha) / 255, alpha);
			}
		}
		cairo_surface_mark_dirty_rectangle(data->image->surface, 0, 0,
		                                   data->image->width,
		                                   data->image->height);
		data->sampling_number = state.sampling_number;
		updated = true;
	}
	info.GetReturnValue().Set(Boolean::New(iso, updated));
}

} // namespace

void nx_init_irs(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "irsInit", nx_irs_initialize);
	NX_SET_FUNC(init_obj, "irsSensorNew", nx_irs_sensor_new);
	NX_SET_FUNC(init_obj, "irsSensorStart", nx_irs_sensor_start);
	NX_SET_FUNC(init_obj, "irsSensorStop", nx_irs_sensor_stop);
	NX_SET_FUNC(init_obj, "irsSensorUpdate", nx_irs_sensor_update);
}
