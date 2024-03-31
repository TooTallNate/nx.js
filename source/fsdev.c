#include "error.h"
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
	FsSaveDataType type = 0;
	if (
		JS_ToUint32(ctx, &type, argv[0]) ||
		JS_ToBigInt64(ctx, (s64 *)&uid.uid[0], JS_GetPropertyUint32(ctx, argv[2], 0)) ||
		JS_ToBigInt64(ctx, (s64 *)&uid.uid[1], JS_GetPropertyUint32(ctx, argv[2], 1)))
	{
		return JS_EXCEPTION;
	}

	uint32_t cache_index = 0;
	if (argc > 3)
	{
		if (JS_ToUint32(ctx, &cache_index, argv[3]))
		{
			return JS_EXCEPTION;
		}
	}

	size_t nacp_size;
	NacpStruct *nacp = (NacpStruct *)JS_GetArrayBuffer(ctx, &nacp_size, argv[1]);
	if (nacp_size != sizeof(NacpStruct))
	{
		return JS_ThrowTypeError(ctx, "Invalid NACP buffer (got %ld bytes, expected %ld)", nacp_size, sizeof(NacpStruct));
	}

	FsSaveDataAttribute attr;
	memset(&attr, 0, sizeof(attr));
	attr.application_id = nacp->save_data_owner_id;
	attr.uid = uid;
	attr.system_save_data_id = 0;
	attr.save_data_type = type;
	attr.save_data_rank = 0;
	attr.save_data_index = (u16)cache_index;

	FsSaveDataCreationInfo crt;
	memset(&crt, 0, sizeof(crt));
	crt.available_size = 0x4000;
	crt.owner_id = type == FsSaveDataType_Bcat ? 0x010000000000000C : nacp->save_data_owner_id;
	crt.flags = 0;
	crt.save_data_space_id = FsSaveDataSpaceId_User;
	int64_t save_size = 0, journal_size = 0;
	switch (type)
	{
	case FsSaveDataType_Account:
		save_size = nacp->user_account_save_data_size;
		journal_size = nacp->user_account_save_data_journal_size;
		break;

	case FsSaveDataType_Device:
		save_size = nacp->device_save_data_size;
		journal_size = nacp->device_save_data_journal_size;
		break;

	case FsSaveDataType_Bcat:
		save_size = nacp->bcat_delivery_cache_storage_size;
		journal_size = nacp->bcat_delivery_cache_storage_size;
		break;

	case FsSaveDataType_Cache:
		save_size = nacp->cache_storage_size;
		if (!save_size)
		{
			save_size = 32 * 1024 * 1024;
		}
		crt.save_data_space_id = FsSaveDataSpaceId_SdUser;
		if (nacp->cache_storage_journal_size > nacp->cache_storage_data_and_journal_size_max)
			journal_size = nacp->cache_storage_journal_size;
		else
			journal_size = nacp->cache_storage_data_and_journal_size_max;
		break;

	default:
		JS_ThrowInternalError(ctx, "Unsupported type: %d", type);
		return JS_EXCEPTION;
		break;
	}
	crt.journal_size = journal_size;
	crt.save_data_size = save_size;

	FsSaveDataMetaInfo meta;
	memset(&meta, 0, sizeof(meta));
	if (type != FsSaveDataType_Bcat)
	{
		meta.size = 0x40060;
		meta.type = FsSaveDataMetaType_Thumbnail;
	}

	Result rc = fsCreateSaveDataFileSystem(&attr, &crt, &meta);
	if (R_FAILED(rc))
	{
		return nx_throw_libnx_error(ctx, rc, "fsCreateSaveDataFileSystem()");
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
		return nx_throw_libnx_error(ctx, rc, "fsdevMountSaveData()");
	}
	return JS_UNDEFINED;
}

static JSValue nx_fsdev_mount_cache_storage(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 save_data_index = 0;
	if (JS_ToUint32(ctx, &save_data_index, argv[2]))
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

	Result rc = fsdevMountCacheStorage(name, nacp->save_data_owner_id, (u16)save_data_index);
	JS_FreeCString(ctx, name);
	if (R_FAILED(rc))
	{
		return nx_throw_libnx_error(ctx, rc, "fsdevMountCacheStorage()");
	}
	return JS_UNDEFINED;
}

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
	JS_CFUNC_DEF("fsdevCreateSaveData", 3, nx_fsdev_create_save_data),
	JS_CFUNC_DEF("fsdevMountSaveData", 3, nx_fsdev_mount_save_data),
	JS_CFUNC_DEF("fsdevMountCacheStorage", 3, nx_fsdev_mount_cache_storage),
	JS_CFUNC_DEF("fsdevUnmountDevice", 1, nx_fsdev_unmount_device),
};

void nx_init_fsdev(JSContext *ctx, JSValueConst init_obj)
{
	JS_SetPropertyFunctionList(ctx, init_obj, function_list, countof(function_list));
}
