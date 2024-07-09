#include "ns.h"
#include "applet.h"

static JSClassID nx_app_class_id;

nx_app_t *nx_get_app(JSContext *ctx, JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_app_class_id);
}

static void finalizer_app(JSRuntime *rt, JSValue val) {
	nx_app_t *app = JS_GetOpaque(val, nx_app_class_id);
	if (app) {
		js_free_rt(rt, app);
	}
}

static JSValue nx_ns_exit(JSContext *ctx, JSValueConst this_val, int argc,
						  JSValueConst *argv) {
	nsExit();
	return JS_UNDEFINED;
}

static JSValue nx_ns_initialize(JSContext *ctx, JSValueConst this_val, int argc,
								JSValueConst *argv) {
	Result rc = nsInitialize();
	if (R_FAILED(rc)) {
		JS_ThrowInternalError(ctx, "nsInitialize() returned 0x%x", rc);
		return JS_EXCEPTION;
	}
	return JS_NewCFunction(ctx, nx_ns_exit, "nsExit", 0);
}

static JSValue nx_ns_app_new(JSContext *ctx, JSValueConst this_val, int argc,
							 JSValueConst *argv) {
	nx_app_t *data = js_mallocz(ctx, sizeof(nx_app_t));
	if (!data) {
		return JS_EXCEPTION;
	}
	bool loaded = false;
	u64 application_id = 0;
	if (JS_IsNull(argv[0])) {
		// Get own app ID from the running process.
		// This is the case for NSP installations, or when running via an
		// emulator (i.e. Ryujinx).
		Result rc = svcGetInfo(&application_id, InfoType_ProgramId,
							   CUR_PROCESS_HANDLE, 0);
		if (R_FAILED(rc)) {
			JS_ThrowInternalError(ctx, "svcGetInfo() returned 0x%x", rc);
			return JS_EXCEPTION;
		}
	} else if (JS_IsBigInt(ctx, argv[0])) {
		if (JS_ToBigInt64(ctx, (int64_t *)&application_id, argv[0])) {
			return JS_EXCEPTION;
		}
	} else {
		// Get app ID from parsing the array buffer which contains an NRO file.
		// This is the case when running the NRO through hbmenu or a forwarder.
		size_t nro_size;
		uint8_t *nro = JS_GetArrayBuffer(ctx, &nro_size, argv[0]);
		u32 asset_header_offset = *(u32 *)(nro + 0x18);
		uint8_t *asset_header = nro + asset_header_offset;
		u32 icon_section_offset = *(u32 *)(asset_header + 0x8);
		u32 icon_section_size = *(u32 *)(asset_header + 0x10);
		u32 nacp_section_offset = *(u32 *)(asset_header + 0x18);
		u32 nacp_section_size = *(u32 *)(asset_header + 0x20);
		memcpy(&data->data.icon, asset_header + icon_section_offset,
			   icon_section_size);
		memcpy(&data->data.nacp, asset_header + nacp_section_offset,
			   nacp_section_size);
		data->icon_size = icon_section_size;
		loaded = true;
	}

	if (!loaded) {
		size_t outSize;
		Result rc = nsGetApplicationControlData(
			NsApplicationControlSource_Storage, application_id, &data->data,
			sizeof(data->data), &outSize);
		if (R_FAILED(rc)) {
			return JS_ThrowInternalError(
				ctx, "nsGetApplicationControlData() returned 0x%x", rc);
		}
		data->icon_size = outSize > 0 ? outSize - sizeof(data->data.nacp) : 0;
	}

	JSValue app = JS_NewObjectClass(ctx, nx_app_class_id);
	JS_SetOpaque(app, data);
	return app;
}

static JSValue nx_ns_app_id(JSContext *ctx, JSValueConst this_val, int argc,
							JSValueConst *argv) {
	nx_app_t *app = nx_get_app(ctx, this_val);
	if (!app) {
		return JS_EXCEPTION;
	}
	return JS_NewBigUint64(ctx, app->data.nacp.presence_group_id);
}

static JSValue nx_ns_app_nacp(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	nx_app_t *app = nx_get_app(ctx, this_val);
	if (!app) {
		return JS_EXCEPTION;
	}
	return JS_NewArrayBufferCopy(ctx, (uint8_t *)&app->data.nacp,
								 sizeof(app->data.nacp));
}

static JSValue nx_ns_app_icon(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	nx_app_t *app = nx_get_app(ctx, this_val);
	if (!app) {
		return JS_EXCEPTION;
	}
	if (app->icon_size <= 0) {
		return JS_UNDEFINED;
	}
	return JS_NewArrayBufferCopy(ctx, (uint8_t *)&app->data.icon,
								 app->icon_size);
}

static JSValue nx_ns_app_name(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	nx_app_t *app = nx_get_app(ctx, this_val);
	if (!app) {
		return JS_EXCEPTION;
	}
	NacpLanguageEntry *langEntry;
	Result rc = nacpGetLanguageEntry(&app->data.nacp, &langEntry);
	if (R_FAILED(rc) || !langEntry) {
		JS_ThrowInternalError(ctx, "nacpGetLanguageEntry() returned 0x%x", rc);
		return JS_EXCEPTION;
	}
	return JS_NewString(ctx, langEntry->name);
}

static JSValue nx_ns_app_author(JSContext *ctx, JSValueConst this_val, int argc,
								JSValueConst *argv) {
	nx_app_t *app = nx_get_app(ctx, this_val);
	if (!app) {
		return JS_EXCEPTION;
	}
	NacpLanguageEntry *langEntry;
	Result rc = nacpGetLanguageEntry(&app->data.nacp, &langEntry);
	if (R_FAILED(rc) || !langEntry) {
		JS_ThrowInternalError(ctx, "nacpGetLanguageEntry() returned 0x%x", rc);
		return JS_EXCEPTION;
	}
	return JS_NewString(ctx, langEntry->author);
}

static JSValue nx_ns_app_next(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	NsApplicationRecord record;
	int record_count = 0;

	int offset;
	if (JS_ToInt32(ctx, &offset, argv[0])) {
		return JS_EXCEPTION;
	}

	Result rc = nsListApplicationRecord(&record, 1, offset, &record_count);
	if (R_FAILED(rc)) {
		JS_ThrowInternalError(ctx, "nsListApplicationRecord() returned 0x%x",
							  rc);
		return JS_EXCEPTION;
	}

	if (!record_count) {
		return JS_NULL;
	}

	return JS_NewBigInt64(ctx, record.application_id);
}

static JSValue nx_ns_app_version(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	nx_app_t *app = nx_get_app(ctx, this_val);
	if (!app) {
		return JS_EXCEPTION;
	}
	return JS_NewString(ctx, app->data.nacp.display_version);
}

static JSValue nx_ns_app_init(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_GET(proto, "id", nx_ns_app_id);
	NX_DEF_GET(proto, "nacp", nx_ns_app_nacp);
	NX_DEF_GET(proto, "icon", nx_ns_app_icon);
	NX_DEF_GET(proto, "name", nx_ns_app_name);
	NX_DEF_GET(proto, "author", nx_ns_app_author);
	NX_DEF_GET(proto, "version", nx_ns_app_version);
	NX_DEF_FUNC(proto, "launch", nx_appletRequestLaunchApplication, 0);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("nsInitialize", 0, nx_ns_initialize),
	JS_CFUNC_DEF("nsAppNew", 1, nx_ns_app_new),
	JS_CFUNC_DEF("nsAppInit", 1, nx_ns_app_init),
	JS_CFUNC_DEF("nsAppNext", 1, nx_ns_app_next),
};

void nx_init_ns(JSContext *ctx, JSValueConst init_obj) {
	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_app_class_id);
	JSClassDef app_class = {
		"Application",
		.finalizer = finalizer_app,
	};
	JS_NewClass(rt, nx_app_class_id, &app_class);

	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
