#include "fsdev.h"

void strip_trailing_colon(char *str)
{
	size_t len = strlen(str);
	if (str[len - 1] == ':')
	{
		str[len - 1] = '\0';
	}
}

static JSValue nx_fsdev_create_save_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	AccountUid uid;
	if (
		JS_ToBigInt64(ctx, (s64 *)&uid.uid[0], JS_GetPropertyUint32(ctx, argv[1], 0)) ||
		JS_ToBigInt64(ctx, (s64 *)&uid.uid[1], JS_GetPropertyUint32(ctx, argv[1], 1)))
	{
		return JS_EXCEPTION;
	}

	size_t nacp_size;
	NacpStruct *nacp = (NacpStruct *)JS_GetArrayBuffer(ctx, &nacp_size, argv[0]);
	if (nacp_size != sizeof(NacpStruct))
	{
		return JS_ThrowTypeError(ctx, "Invalid NACP buffer (got %ld bytes, expected %ld)", nacp_size, sizeof(NacpStruct));
	}

	FsSaveDataAttribute attr;
	FsSaveDataCreationInfo info;
	FsSaveDataMetaInfo meta;
	memset(&attr, 0, sizeof(attr));
	memset(&info, 0, sizeof(info));
	memset(&meta, 0, sizeof(meta));

	attr.application_id = nacp->save_data_owner_id;
	attr.uid = uid;
	attr.system_save_data_id = 0;
	attr.save_data_type = FsSaveDataType_Account;
	attr.save_data_rank = 0;
	attr.save_data_index = 0;

	info.journal_size = nacp->user_account_save_data_journal_size;
	info.save_data_size = nacp->user_account_save_data_size;
	info.available_size = 0x4000;
	info.owner_id = nacp->save_data_owner_id;
	info.flags = 0;
	info.save_data_space_id = FsSaveDataSpaceId_User;

	meta.size = 0x40060;
	meta.type = FsSaveDataMetaType_Thumbnail;

	Result rc = fsCreateSaveDataFileSystem(&attr, &info, &meta);
	if (R_FAILED(rc))
	{
		JSValue err = JS_NewError(ctx);
		u32 module = R_MODULE(rc);
		u32 desc = R_DESCRIPTION(rc);
		char message[256];
		snprintf(message, 256, "fsCreateSaveDataFileSystem() failed (module: %u, description: %u)", module, desc);
		JS_DefinePropertyValueStr(ctx, err, "message", JS_NewString(ctx, message), JS_PROP_C_W);
		JS_SetPropertyStr(ctx, err, "module", JS_NewUint32(ctx, module));
		JS_SetPropertyStr(ctx, err, "description", JS_NewUint32(ctx, desc));
		JS_SetPropertyStr(ctx, err, "value", JS_NewUint32(ctx, R_VALUE(rc)));
		return JS_Throw(ctx, err);
	}

	return JS_UNDEFINED;
}

static JSValue nx_fsdev_mount_save_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	AccountUid uid;
	if (
		JS_ToBigInt64(ctx, (s64 *)&uid.uid[0], JS_GetPropertyUint32(ctx, argv[2], 0)) ||
		JS_ToBigInt64(ctx, (s64 *)&uid.uid[1], JS_GetPropertyUint32(ctx, argv[2], 1)))
	{
		return JS_EXCEPTION;
	}
	size_t nacp_size;
	NacpStruct *nacp = (NacpStruct *)JS_GetArrayBuffer(ctx, &nacp_size, argv[1]);
	if (nacp_size != sizeof(NacpStruct))
	{
		return JS_ThrowTypeError(ctx, "Invalid NACP buffer (got %ld bytes, expected %ld)", nacp_size, sizeof(NacpStruct));
	}
	const char *name = JS_ToCString(ctx, argv[0]);
	if (!name)
	{
		return JS_EXCEPTION;
	}
	strip_trailing_colon((char *)name);
	Result rc = fsdevMountSaveData(name, nacp->save_data_owner_id, uid);
	JS_FreeCString(ctx, name);
	if (R_FAILED(rc))
	{
		JSValue err = JS_NewError(ctx);
		u32 module = R_MODULE(rc);
		u32 desc = R_DESCRIPTION(rc);
		char message[256];
		snprintf(message, 256, "fsdevMountSaveData() failed (module: %u, description: %u)", module, desc);
		JS_DefinePropertyValueStr(ctx, err, "message", JS_NewString(ctx, message), JS_PROP_C_W);
		JS_SetPropertyStr(ctx, err, "module", JS_NewUint32(ctx, module));
		JS_SetPropertyStr(ctx, err, "description", JS_NewUint32(ctx, desc));
		JS_SetPropertyStr(ctx, err, "value", JS_NewUint32(ctx, R_VALUE(rc)));
		// JSValue argv_arr = JS_NewArray(ctx);
		// for (int i = 0; i < argc; i++) {
		//	JS_SetPropertyUint32(ctx, argv_arr, i, JS_DupValue(ctx, argv[i]));
		// }
		// JS_SetPropertyStr(ctx, err, "argv", argv_arr);
		return JS_Throw(ctx, err);
	}
	return JS_UNDEFINED;
}

// static JSValue nx_fsdev_mount_cache_storage(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
//{
//	size_t nacp_size;
//	NacpStruct *nacp = (NacpStruct *)JS_GetArrayBuffer(ctx, &nacp_size, argv[0]);
//	if (nacp_size != sizeof(NacpStruct)) {
//		return JS_ThrowTypeError(ctx, "Invalid NACP buffer (got %ld bytes, expected %ld)", nacp_size, sizeof(NacpStruct));
//	}
//	const char *name = JS_ToCString(ctx, argv[0]);
//	if (!name)
//	{
//		return JS_EXCEPTION;
//	}
//	strip_trailing_colon(name);
//	Result rc = fsdevMountCacheStorage(name, nacp->cache, uid);
//	JS_FreeCString(ctx, name);
//	if (R_FAILED(rc))
//	{
//		JSValue err = JS_NewError(ctx);
//		u32 module = R_MODULE(rc);
//		u32 desc = R_DESCRIPTION(rc);
//		char message[256];
//		snprintf(message, 256, "fsdevMountSaveData() failed (module: %u, description: %u)", module, desc);
//		JS_DefinePropertyValueStr(ctx, err, "message", JS_NewString(ctx, message), JS_PROP_C_W);
//		JS_SetPropertyStr(ctx, err, "module", JS_NewUint32(ctx, module));
//		JS_SetPropertyStr(ctx, err, "description", JS_NewUint32(ctx, desc));
//		JS_SetPropertyStr(ctx, err, "value", JS_NewUint32(ctx, R_VALUE(rc)));
//		//JSValue argv_arr = JS_NewArray(ctx);
//		//for (int i = 0; i < argc; i++) {
//		//	JS_SetPropertyUint32(ctx, argv_arr, i, JS_DupValue(ctx, argv[i]));
//		//}
//		//JS_SetPropertyStr(ctx, err, "argv", argv_arr);
//		return JS_Throw(ctx, err);
//	}
//	return JS_UNDEFINED;
// }

static JSValue nx_fsdev_commit_device(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *name = JS_ToCString(ctx, argv[0]);
	if (!name)
	{
		return JS_EXCEPTION;
	}
	strip_trailing_colon((char *)name);
	Result rc = fsdevCommitDevice(name);
	JS_FreeCString(ctx, name);
	if (R_FAILED(rc))
	{
		JS_ThrowInternalError(ctx, "fsdevCommitDevice() returned 0x%x", rc);
		return JS_EXCEPTION;
	}
	return JS_UNDEFINED;
}

static JSValue nx_fsdev_unmount_device(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *name = JS_ToCString(ctx, argv[0]);
	if (!name)
	{
		return JS_EXCEPTION;
	}
	strip_trailing_colon((char *)name);
	Result rc = fsdevUnmountDevice(name);
	JS_FreeCString(ctx, name);
	if (R_FAILED(rc))
	{
		JS_ThrowInternalError(ctx, "fsdevUnmountDevice() returned 0x%x", rc);
		return JS_EXCEPTION;
	}
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("fsdevCommitDevice", 0, nx_fsdev_commit_device),
	JS_CFUNC_DEF("fsdevCreateSaveData", 0, nx_fsdev_create_save_data),
	JS_CFUNC_DEF("fsdevMountSaveData", 0, nx_fsdev_mount_save_data),
	JS_CFUNC_DEF("fsdevUnmountDevice", 1, nx_fsdev_unmount_device),
};

void nx_init_fsdev(JSContext *ctx, JSValueConst init_obj)
{
	JS_SetPropertyFunctionList(ctx, init_obj, function_list, countof(function_list));
}
