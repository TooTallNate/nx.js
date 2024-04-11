#include "error.h"
#include "fsdev.h"

static JSClassID nx_save_data_class_id;
static JSClassID nx_save_data_iterator_class_id;

typedef struct
{
	bool info_loaded;
	FsSaveDataInfo info;
	FsFileSystem *fs;
	const char *mount_name;
} nx_save_data_t;

typedef struct
{
	FsSaveDataInfoReader it;
} nx_save_data_iterator_t;

static void finalizer_save_data(JSRuntime *rt, JSValue val)
{
	nx_save_data_t *save_data = JS_GetOpaque(val, nx_save_data_class_id);
	if (save_data)
	{
		if (save_data->mount_name)
		{
			fsdevUnmountDevice(save_data->mount_name);
			js_free_rt(rt, (void *)save_data->mount_name);
			save_data->mount_name = NULL;
		}
		if (save_data->fs)
		{
			fsFsClose(save_data->fs);
			save_data->fs = NULL;
		}
		js_free_rt(rt, save_data);
	}
}

static void finalizer_save_data_iterator(JSRuntime *rt, JSValue val)
{
	nx_save_data_iterator_t *save_data_iterator = JS_GetOpaque(val, nx_save_data_iterator_class_id);
	if (save_data_iterator)
	{
		fsSaveDataInfoReaderClose(&save_data_iterator->it);
		js_free_rt(rt, save_data_iterator);
	}
}

void strip_trailing_colon(char *str)
{
	size_t len = strlen(str);
	if (str[len - 1] == ':')
	{
		str[len - 1] = '\0';
	}
}

static JSValue nx_save_data_new(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Result rc;
	u32 space_id;
	nx_save_data_t *save_data = js_mallocz(ctx, sizeof(nx_save_data_t));
	if (!save_data || JS_ToUint32(ctx, &space_id, argv[0]))
	{
		return JS_EXCEPTION;
	}
	s64 total = 0;
	bool found = false;
	FsSaveDataInfo info = {0};
	FsSaveDataInfoReader it = {0};
	if (JS_IsBigInt(ctx, argv[1]))
	{
		u64 id;
		if (JS_ToBigInt64(ctx, (s64 *)&id, argv[1]))
		{
			js_free(ctx, save_data);
			return JS_EXCEPTION;
		}
		rc = fsOpenSaveDataInfoReader(&it, (FsSaveDataSpaceId)space_id);
		if (R_SUCCEEDED(rc))
		{
			while (!found && R_SUCCEEDED(fsSaveDataInfoReaderRead(&it, &info, 1, &total)) && total != 0)
			{
				if (info.save_data_id == id)
				{
					found = true;
				}
			}
		}
	}
	else
	{
		FsSaveDataFilter filter = {0};

		// "type"
		JSValue type_val = JS_GetPropertyStr(ctx, argv[1], "type");
		if (JS_IsNumber(type_val))
		{
			u32 type;
			if (JS_ToUint32(ctx, &type, type_val))
			{
				js_free(ctx, save_data);
				return JS_EXCEPTION;
			}
			filter.attr.save_data_type = type;
			filter.filter_by_save_data_type = true;
		}
		JS_FreeValue(ctx, type_val);

		// "uid"
		JSValue uid_val = JS_GetPropertyStr(ctx, argv[1], "uid");
		if (JS_IsArray(ctx, uid_val))
		{
			if (
				JS_ToBigInt64(ctx, (int64_t *)&filter.attr.uid.uid[0], JS_GetPropertyUint32(ctx, uid_val, 0)) ||
				JS_ToBigInt64(ctx, (int64_t *)&filter.attr.uid.uid[1], JS_GetPropertyUint32(ctx, uid_val, 1)))
			{
				js_free(ctx, save_data);
				return JS_EXCEPTION;
			}
			filter.filter_by_user_id = true;
		}
		JS_FreeValue(ctx, uid_val);

		// "systemId"
		JSValue system_id_val = JS_GetPropertyStr(ctx, argv[1], "systemId");
		if (JS_IsBigInt(ctx, system_id_val))
		{
			u64 system_id;
			if (JS_ToBigInt64(ctx, (s64 *)&system_id, system_id_val))
			{
				js_free(ctx, save_data);
				return JS_EXCEPTION;
			}
			filter.attr.system_save_data_id = system_id;
			filter.filter_by_system_save_data_id = true;
		}
		JS_FreeValue(ctx, system_id_val);

		// "applicationId"
		JSValue application_id_val = JS_GetPropertyStr(ctx, argv[1], "applicationId");
		if (JS_IsBigInt(ctx, application_id_val))
		{
			u64 application_id;
			if (JS_ToBigInt64(ctx, (s64 *)&application_id, application_id_val))
			{
				js_free(ctx, save_data);
				return JS_EXCEPTION;
			}
			filter.attr.application_id = application_id;
			filter.filter_by_application_id = true;
		}
		JS_FreeValue(ctx, application_id_val);

		// "index"
		JSValue index_val = JS_GetPropertyStr(ctx, argv[1], "index");
		if (JS_IsNumber(index_val))
		{
			u32 index;
			if (JS_ToUint32(ctx, &index, index_val))
			{
				js_free(ctx, save_data);
				return JS_EXCEPTION;
			}
			filter.attr.save_data_index = index;
			filter.filter_by_index = true;
		}
		JS_FreeValue(ctx, index_val);

		// "rank"
		JSValue rank_val = JS_GetPropertyStr(ctx, argv[1], "rank");
		if (JS_IsNumber(rank_val))
		{
			u32 rank;
			if (JS_ToUint32(ctx, &rank, rank_val))
			{
				js_free(ctx, save_data);
				return JS_EXCEPTION;
			}
			filter.attr.save_data_rank = rank;
			filter.save_data_rank = rank;
		}
		JS_FreeValue(ctx, rank_val);

		rc = fsOpenSaveDataInfoReaderWithFilter(&it, (FsSaveDataSpaceId)space_id, &filter);
		if (R_FAILED(rc))
		{
			js_free(ctx, save_data);
			return nx_throw_libnx_error(ctx, rc, "fsOpenSaveDataInfoReaderWithFilter()");
		}

		rc = fsSaveDataInfoReaderRead(&it, &info, 1, &total);
		if (R_FAILED(rc))
		{
			js_free(ctx, save_data);
			return nx_throw_libnx_error(ctx, rc, "fsSaveDataInfoReaderRead()");
		}

		found = total != 0;
	}

	if (!found)
	{
		js_free(ctx, save_data);
		return JS_ThrowReferenceError(ctx, "Save data not found");
	}

	save_data->info = info;
	JSValue obj = JS_NewObjectClass(ctx, nx_save_data_class_id);
	JS_SetOpaque(obj, save_data);
	JS_SetPropertyStr(ctx, obj, "url", JS_NULL);
	return obj;
}

static JSValue nx_save_data_id(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_save_data_t *save_data = JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data)
	{
		return JS_EXCEPTION;
	}
	return JS_NewBigUint64(ctx, save_data->info.save_data_id);
}

static JSValue nx_save_data_space_id(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_save_data_t *save_data = JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data)
	{
		return JS_EXCEPTION;
	}
	return JS_NewUint32(ctx, save_data->info.save_data_space_id);
}

static JSValue nx_save_data_type(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_save_data_t *save_data = JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data)
	{
		return JS_EXCEPTION;
	}
	return JS_NewUint32(ctx, save_data->info.save_data_type);
}

static JSValue nx_save_data_uid(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_save_data_t *save_data = JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data)
	{
		return JS_EXCEPTION;
	}
	JSValue uid = JS_NewArray(ctx);
	JS_SetPropertyUint32(ctx, uid, 0, JS_NewBigUint64(ctx, save_data->info.uid.uid[0]));
	JS_SetPropertyUint32(ctx, uid, 1, JS_NewBigUint64(ctx, save_data->info.uid.uid[1]));
	return uid;
}

static JSValue nx_save_data_system_id(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_save_data_t *save_data = JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data)
	{
		return JS_EXCEPTION;
	}
	return JS_NewBigUint64(ctx, save_data->info.system_save_data_id);
}

static JSValue nx_save_data_application_id(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_save_data_t *save_data = JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data)
	{
		return JS_EXCEPTION;
	}
	return JS_NewBigUint64(ctx, save_data->info.application_id);
}

static JSValue nx_save_data_size(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_save_data_t *save_data = JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data)
	{
		return JS_EXCEPTION;
	}
	return JS_NewBigUint64(ctx, save_data->info.size);
}

static JSValue nx_save_data_index(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_save_data_t *save_data = JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data)
	{
		return JS_EXCEPTION;
	}
	return JS_NewUint32(ctx, save_data->info.save_data_index);
}

static JSValue nx_save_data_rank(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_save_data_t *save_data = JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data)
	{
		return JS_EXCEPTION;
	}
	return JS_NewUint32(ctx, save_data->info.save_data_rank);
}

static JSValue nx_save_data_commit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_save_data_t *save_data = JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data)
	{
		return JS_EXCEPTION;
	}
	if (!save_data->fs)
	{
		return JS_ThrowTypeError(ctx, "SaveData is not mounted");
	}
	Result rc = fsFsCommit(save_data->fs);
	if (R_FAILED(rc))
	{
		return nx_throw_libnx_error(ctx, rc, "fsFsCommit()");
	}
	return JS_UNDEFINED;
}

static JSValue nx_save_data_delete(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_save_data_t *save_data = JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data)
	{
		return JS_EXCEPTION;
	}
	Result rc = fsDeleteSaveDataFileSystemBySaveDataSpaceId(
		save_data->info.save_data_space_id,
		save_data->info.save_data_id);
	if (R_FAILED(rc))
	{
		return nx_throw_libnx_error(ctx, rc, "fsDeleteSaveDataFileSystemBySaveDataSpaceId()");
	}
	return JS_UNDEFINED;
}

static JSValue nx_save_data_extend(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	s64 data_size;
	s64 journal_size;
	nx_save_data_t *save_data = JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data ||
		JS_ToBigInt64(ctx, &data_size, argv[2]) ||
		JS_ToBigInt64(ctx, &journal_size, argv[3]))
	{
		return JS_EXCEPTION;
	}
	Result rc = fsExtendSaveDataFileSystem(
		save_data->info.save_data_space_id,
		save_data->info.save_data_id,
		data_size,
		journal_size);
	if (R_FAILED(rc))
	{
		return nx_throw_libnx_error(ctx, rc, "fsExtendSaveDataFileSystem()");
	}
	return JS_UNDEFINED;
}

static JSValue nx_save_data_unmount(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_save_data_t *save_data = JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data)
	{
		return JS_EXCEPTION;
	}
	if (!save_data->mount_name)
	{
		return JS_ThrowTypeError(ctx, "SaveData is not mounted");
	}
	Result rc = fsdevUnmountDevice(save_data->mount_name);
	if (R_FAILED(rc))
	{
		return nx_throw_libnx_error(ctx, rc, "fsdevUnmountDevice()");
	}
	js_free(ctx, (void *)save_data->mount_name);
	save_data->mount_name = NULL;
	JS_SetPropertyStr(ctx, this_val, "url", JS_NULL);
	return JS_UNDEFINED;
}

static JSValue nx_fs_open_save_data_info_reader(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	u32 space_id;
	if (JS_ToUint32(ctx, &space_id, argv[0]))
	{
		return JS_EXCEPTION;
	}
	nx_save_data_iterator_t *save_data_iterator = js_mallocz(ctx, sizeof(nx_save_data_iterator_t));
	if (!save_data_iterator)
	{
		return JS_EXCEPTION;
	}
	Result rc = fsOpenSaveDataInfoReader(&save_data_iterator->it, (FsSaveDataSpaceId)space_id);
	if (R_FAILED(rc))
	{
		return JS_NULL;
	}
	JSValue it = JS_NewObjectClass(ctx, nx_save_data_iterator_class_id);
	JS_SetOpaque(it, save_data_iterator);
	return it;
}

static JSValue nx_fs_save_data_info_reader_next(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_save_data_iterator_t *save_data_iterator = JS_GetOpaque2(ctx, argv[0], nx_save_data_iterator_class_id);
	if (!save_data_iterator)
	{
		return JS_EXCEPTION;
	}
	nx_save_data_t *save_data = js_mallocz(ctx, sizeof(nx_save_data_t));
	if (!save_data)
	{
		return JS_EXCEPTION;
	}
	s64 total = 0;
	Result rc = fsSaveDataInfoReaderRead(&save_data_iterator->it, &save_data->info, 1, &total);
	if (R_FAILED(rc))
	{
		js_free(ctx, save_data);
		return nx_throw_libnx_error(ctx, rc, "fsSaveDataInfoReaderRead()");
	}
	if (total == 0)
	{
		js_free(ctx, save_data);
		return JS_NULL;
	}
	JSValue obj = JS_NewObjectClass(ctx, nx_save_data_class_id);
	JS_SetOpaque(obj, save_data);
	JS_SetPropertyStr(ctx, obj, "url", JS_NULL);
	return obj;
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
		JS_ThrowTypeError(ctx, "Unsupported type: %d", type);
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

static JSValue nx_fsdev_mount(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	return JS_UNDEFINED;
}

static JSValue nx_save_data_init(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_GET(proto, "id", nx_save_data_id);
	NX_DEF_GET(proto, "spaceId", nx_save_data_space_id);
	NX_DEF_GET(proto, "type", nx_save_data_type);
	NX_DEF_GET(proto, "uid", nx_save_data_uid);
	NX_DEF_GET(proto, "systemId", nx_save_data_system_id);
	NX_DEF_GET(proto, "applicationId", nx_save_data_application_id);
	NX_DEF_GET(proto, "size", nx_save_data_size);
	NX_DEF_GET(proto, "index", nx_save_data_index);
	NX_DEF_GET(proto, "rank", nx_save_data_rank);
	NX_DEF_FUNC(proto, "commit", nx_save_data_commit, 0);
	NX_DEF_FUNC(proto, "delete", nx_save_data_delete, 0);
	NX_DEF_FUNC(proto, "extend", nx_save_data_extend, 2);
	NX_DEF_FUNC(proto, "unmount", nx_save_data_unmount, 0);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("saveDataInit", 1, nx_save_data_init),
	JS_CFUNC_DEF("saveDataNew", 1, nx_save_data_new),
	JS_CFUNC_DEF("fsOpenSaveDataInfoReader", 1, nx_fs_open_save_data_info_reader),
	JS_CFUNC_DEF("fsSaveDataInfoReaderNext", 1, nx_fs_save_data_info_reader_next),
	JS_CFUNC_DEF("fsdevCreateSaveData", 3, nx_fsdev_create_save_data),
	JS_CFUNC_DEF("fsdevMount", 2, nx_fsdev_mount),
};

void nx_init_fsdev(JSContext *ctx, JSValueConst init_obj)
{
	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_save_data_class_id);
	JSClassDef save_data_class = {
		"SaveData",
		.finalizer = finalizer_save_data,
	};
	JS_NewClass(rt, nx_save_data_class_id, &save_data_class);

	JS_NewClassID(rt, &nx_save_data_iterator_class_id);
	JSClassDef save_data_iterator_class = {
		"SaveDataIterator",
		.finalizer = finalizer_save_data_iterator,
	};
	JS_NewClass(rt, nx_save_data_iterator_class_id, &save_data_iterator_class);

	JS_SetPropertyFunctionList(ctx, init_obj, function_list, countof(function_list));
}
