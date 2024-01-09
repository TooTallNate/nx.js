#include "audren.h"

static const AudioRendererConfig arConfig =
	{
		.output_rate = AudioRendererOutputRate_48kHz,
		.num_voices = 24,
		.num_effects = 0,
		.num_sinks = 1,
		.num_mix_objs = 1,
		.num_mix_buffers = 2,
};

static JSValue nx_audren_exit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	audrenExit();
	return JS_UNDEFINED;
}

static JSValue nx_audren_initialize(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Result rc = audrenInitialize(&arConfig);
	if (R_FAILED(rc))
	{
		JS_ThrowInternalError(ctx, "audrenInitialize() returned 0x%x", rc);
		return JS_EXCEPTION;
	}
	return JS_NewCFunction(ctx, nx_audren_exit, "", 0);
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("audrenInit", 0, nx_audren_initialize),
};

void nx_init_audren(JSContext *ctx, JSValueConst init_obj)
{
	JS_SetPropertyFunctionList(ctx, init_obj, function_list, countof(function_list));
}
