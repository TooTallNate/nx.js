#include "applet.h"
#include "ns.h"

static JSValue nx_ns_exit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nsExit();
	return JS_UNDEFINED;
}

static JSValue nx_ns_initialize(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Result rc = nsInitialize();
	if (R_FAILED(rc))
	{
		JS_ThrowInternalError(ctx, "nsInitialize() returned 0x%x", rc);
		return JS_EXCEPTION;
	}
	return JS_NewCFunction(ctx, nx_ns_exit, "", 0);
}

static JSValue nx_ns_application_record(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	NsApplicationRecord record;
	int record_count = 0;

	int offset;
	if (JS_ToInt32(ctx, &offset, argv[0]))
	{
		return JS_EXCEPTION;
	}

	Result rc = nsListApplicationRecord(&record, 1, offset, &record_count);
	if (R_FAILED(rc))
	{
		JS_ThrowInternalError(ctx, "nsListApplicationRecord() returned 0x%x", rc);
		return JS_EXCEPTION;
	}

	if (!record_count)
	{
		return JS_NULL;
	}

	JSValue val = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, val, "id", JS_NewBigInt64(ctx, record.application_id));
	JS_SetPropertyStr(ctx, val, "type", JS_NewUint32(ctx, record.type));

	NsApplicationControlData controlData;
	size_t outSize;
	rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, record.application_id, &controlData, sizeof(controlData), &outSize);
	if (R_FAILED(rc))
	{
		JS_ThrowInternalError(ctx, "nsGetApplicationControlData() returned 0x%x", rc);
		return JS_EXCEPTION;
	}
	JS_SetPropertyStr(ctx, val, "nacp", JS_NewArrayBufferCopy(ctx, (const uint8_t *)&controlData.nacp, sizeof(controlData.nacp)));
	JS_SetPropertyStr(ctx, val, "icon", JS_NewArrayBufferCopy(ctx, (const uint8_t *)&controlData.icon, sizeof(controlData.icon)));

	NacpLanguageEntry *langEntry;
	rc = nacpGetLanguageEntry(&controlData.nacp, &langEntry);
	if (R_FAILED(rc))
	{
		JS_ThrowInternalError(ctx, "nacpGetLanguageEntry() returned 0x%x", rc);
		return JS_EXCEPTION;
	}
	if (langEntry != NULL)
	{
		JS_SetPropertyStr(ctx, val, "name", JS_NewString(ctx, langEntry->name));
		JS_SetPropertyStr(ctx, val, "author", JS_NewString(ctx, langEntry->author));
	}

	return val;
}

static JSValue nx_ns_app_init(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_FUNC(proto, "launch", nx_appletRequestLaunchApplication, 0);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("nsInitialize", 0, nx_ns_initialize),
	JS_CFUNC_DEF("nsAppInit", 1, nx_ns_app_init),
	JS_CFUNC_DEF("nsApplicationRecord", 1, nx_ns_application_record),
};

void nx_init_ns(JSContext *ctx, JSValueConst init_obj)
{
	JS_SetPropertyFunctionList(ctx, init_obj, function_list, countof(function_list));
}
