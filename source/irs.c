#include "irs.h"
#include "canvas.h"
#include "image.h"

static JSClassID nx_ir_sensor_class_id;

struct RGBA {
	u8 r;
	u8 g;
	u8 b;
	u8 a;
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

static void finalizer_ir_sensor(JSRuntime *rt, JSValue val) {
	nx_ir_sensor_t *data = JS_GetOpaque(val, nx_ir_sensor_class_id);
	if (data) {
		js_free_rt(rt, data->sensor_buf);
		irsStopImageProcessor(data->irhandle);
		js_free_rt(rt, data);
	}
}

static JSValue nx_irs_exit(JSContext *ctx, JSValueConst this_val, int argc,
						   JSValueConst *argv) {
	irsExit();
	return JS_UNDEFINED;
}

static JSValue nx_irs_initialize(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	Result rc = irsInitialize();
	if (R_FAILED(rc)) {
		JS_ThrowInternalError(ctx, "irsInitialize() returned 0x%x", rc);
		return JS_EXCEPTION;
	}
	return JS_NewCFunction(ctx, nx_irs_exit, "", 0);
}

static JSValue nx_irs_sensor_new(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	JSValue obj = JS_NewObjectClass(ctx, nx_ir_sensor_class_id);
	nx_ir_sensor_t *data = js_mallocz(ctx, sizeof(nx_ir_sensor_t));
	if (!data) {
		return JS_EXCEPTION;
	}
	JS_SetOpaque(obj, data);

	data->sampling_number = -1;
	data->image = nx_get_image(ctx, argv[0]);
	data->sensor_buf_size = data->image->width * data->image->height;
	data->sensor_buf = js_mallocz(ctx, data->sensor_buf_size);
	if (!data->sensor_buf) {
		return JS_EXCEPTION;
	}

	u32 r, g, b;
	double a;
	if (JS_ToUint32(ctx, &r, JS_GetPropertyUint32(ctx, argv[1], 0)) ||
		JS_ToUint32(ctx, &g, JS_GetPropertyUint32(ctx, argv[1], 1)) ||
		JS_ToUint32(ctx, &b, JS_GetPropertyUint32(ctx, argv[1], 2)) ||
		JS_ToFloat64(ctx, &a, JS_GetPropertyUint32(ctx, argv[1], 3))) {
		return JS_EXCEPTION;
	}
	data->color.r = r;
	data->color.g = g;
	data->color.b = b;
	data->color.a = a * 255;

	// TODO: make configurable
	HidNpadIdType id = HidNpadIdType_Handheld;

	Result rc = irsGetIrCameraHandle(&data->irhandle, id);
	if (R_FAILED(rc)) {
		JS_ThrowInternalError(ctx, "irsGetIrCameraHandle() returned 0x%x", rc);
		return JS_EXCEPTION;
	}

	return obj;
}

static JSValue nx_irs_sensor_start(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_ir_sensor_t *data = JS_GetOpaque2(ctx, argv[0], nx_ir_sensor_class_id);
	if (!data)
		return JS_EXCEPTION;

	irsGetDefaultImageTransferProcessorConfig(&data->config);
	Result rc =
		irsRunImageTransferProcessor(data->irhandle, &data->config, 0x100000);
	if (R_FAILED(rc)) {

		JS_ThrowInternalError(
			ctx, "irsRunImageTransferProcessor() returned 0x%x", rc);
		return JS_EXCEPTION;
	}

	return JS_UNDEFINED;
}

static JSValue nx_irs_sensor_stop(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_ir_sensor_t *data = JS_GetOpaque2(ctx, argv[0], nx_ir_sensor_class_id);
	if (!data)
		return JS_EXCEPTION;
	Result rc = irsStopImageProcessor(data->irhandle);
	if (R_FAILED(rc)) {

		JS_ThrowInternalError(ctx, "irsStopImageProcessor() returned 0x%x", rc);
		return JS_EXCEPTION;
	}

	return JS_UNDEFINED;
}

static JSValue nx_irs_sensor_update(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_ir_sensor_t *data = JS_GetOpaque2(ctx, argv[0], nx_ir_sensor_class_id);
	if (!data)
		return JS_EXCEPTION;

	// With the default config the image is updated every few seconds (see
	// above). Likewise, it takes a few seconds for the initial image to become
	// available. This will return an error when no image is available yet.
	IrsImageTransferProcessorState state;
	Result rc = irsGetImageTransferProcessorState(
		data->irhandle, data->sensor_buf, data->sensor_buf_size, &state);

	bool updated = false;
	if (R_SUCCEEDED(rc) && state.sampling_number != data->sampling_number) {
		// Only update image buffer when `irsGetImageTransferProcessorState()`
		// is successful, and `sampling_number` changed.
		u32 x, y;
		for (y = 0; y < data->image->height; y++) {
			for (x = 0; x < data->image->width; x++) {
				// The IR image/camera is sideways with the joycon held flat.
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

	return JS_NewBool(ctx, updated);
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("irsInit", 0, nx_irs_initialize),
	JS_CFUNC_DEF("irsSensorNew", 3, nx_irs_sensor_new),
	JS_CFUNC_DEF("irsSensorStart", 1, nx_irs_sensor_start),
	JS_CFUNC_DEF("irsSensorStop", 1, nx_irs_sensor_stop),
	JS_CFUNC_DEF("irsSensorUpdate", 1, nx_irs_sensor_update),
};

void nx_init_irs(JSContext *ctx, JSValueConst init_obj) {
	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_ir_sensor_class_id);
	JSClassDef nx_ir_sensor_class = {
		"IrSensor",
		.finalizer = finalizer_ir_sensor,
	};
	JS_NewClass(rt, nx_ir_sensor_class_id, &nx_ir_sensor_class);

	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
