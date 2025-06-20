#include "fsdev.h"
#include "error.h"

static JSClassID nx_file_system_class_id;
static JSClassID nx_save_data_class_id;
static JSClassID nx_save_data_iterator_class_id;

typedef struct {
	FsFileSystem fs;
	char mount_name[32];
} nx_file_system_t;

typedef struct {
	bool info_loaded;
	FsSaveDataInfo info;
	FsFileSystem fs;
	const char *mount_name;
} nx_save_data_t;

typedef struct {
	FsSaveDataInfoReader it;
} nx_save_data_iterator_t;

static void finalizer_file_system(JSRuntime *rt, JSValue val) {
	nx_file_system_t *file_system = JS_GetOpaque(val, nx_file_system_class_id);
	if (file_system) {
		if (strlen(file_system->mount_name) > 0) {
			fsdevUnmountDevice(file_system->mount_name);
			fsFsClose(&file_system->fs);
		}
		js_free_rt(rt, file_system);
	}
}

static void finalizer_save_data(JSRuntime *rt, JSValue val) {
	nx_save_data_t *save_data = JS_GetOpaque(val, nx_save_data_class_id);
	if (save_data) {
		if (save_data->mount_name) {
			fsdevUnmountDevice(save_data->mount_name);
			js_free_rt(rt, (void *)save_data->mount_name);
			save_data->mount_name = NULL;
			fsFsClose(&save_data->fs);
		}
		js_free_rt(rt, save_data);
	}
}

static void finalizer_save_data_iterator(JSRuntime *rt, JSValue val) {
	nx_save_data_iterator_t *save_data_iterator =
		JS_GetOpaque(val, nx_save_data_iterator_class_id);
	if (save_data_iterator) {
		fsSaveDataInfoReaderClose(&save_data_iterator->it);
		js_free_rt(rt, save_data_iterator);
	}
}

void strip_trailing_colon(char *str) {
	size_t len = strlen(str);
	if (str[len - 1] == ':') {
		str[len - 1] = '\0';
	}
}

static JSValue nx_fs_mount(JSContext *ctx, JSValueConst this_val, int argc,
						   JSValueConst *argv) {
	nx_file_system_t *file_system =
		JS_GetOpaque2(ctx, argv[0], nx_file_system_class_id);
	if (!file_system) {
		return JS_EXCEPTION;
	}
	const char *name = JS_ToCString(ctx, argv[1]);
	if (!name) {
		return JS_EXCEPTION;
	}
	int r = fsdevMountDevice(name, file_system->fs);
	if (r < 0) {
		JS_FreeCString(ctx, name);
		return nx_throw_libnx_error(ctx, r, "fsdevMountDevice()");
	}
	strncpy(file_system->mount_name, name, sizeof(file_system->mount_name) - 1);
	JS_FreeCString(ctx, name);
	return JS_UNDEFINED;
}

static JSValue nx_fs_open_bis(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	u32 id;
	if (JS_ToUint32(ctx, &id, argv[0])) {
		return JS_EXCEPTION;
	}
	nx_file_system_t *file_system = js_mallocz(ctx, sizeof(nx_file_system_t));
	if (!file_system) {
		return JS_EXCEPTION;
	}
	Result rc = fsOpenBisFileSystem(&file_system->fs, id, "/");
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "fsOpenBisFileSystem()");
	}
	JSValue obj = JS_NewObjectClass(ctx, nx_file_system_class_id);
	JS_SetOpaque(obj, file_system);
	return obj;
}

static JSValue nx_fs_open_sdmc(JSContext *ctx, JSValueConst this_val, int argc,
							   JSValueConst *argv) {
	nx_file_system_t *file_system = js_mallocz(ctx, sizeof(nx_file_system_t));
	if (!file_system) {
		return JS_EXCEPTION;
	}
	Result rc = fsOpenSdCardFileSystem(&file_system->fs);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "fsOpenSdCardFileSystem()");
	}
	JSValue obj = JS_NewObjectClass(ctx, nx_file_system_class_id);
	JS_SetOpaque(obj, file_system);
	return obj;
}

static JSValue nx_fs_open_with_id(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	u64 title_id;
	FsFileSystemType type;
	FsContentAttributes attributes;
	if (JS_ToBigUint64(ctx, &title_id, argv[0]) ||
		JS_ToUint32(ctx, &type, argv[1]) ||
		JS_ToUint32(ctx, &attributes, argv[3])) {
		return JS_EXCEPTION;
	}
	const char *path = JS_ToCString(ctx, argv[2]);
	if (!path) {
		return JS_EXCEPTION;
	}
	nx_file_system_t *file_system = js_mallocz(ctx, sizeof(nx_file_system_t));
	if (!file_system) {
		JS_FreeCString(ctx, path);
		return JS_EXCEPTION;
	}
	Result rc = fsOpenFileSystemWithId(&file_system->fs, title_id, type, path,
									   attributes);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "fsOpenFileSystemWithId()");
	}
	JSValue obj = JS_NewObjectClass(ctx, nx_file_system_class_id);
	JS_SetOpaque(obj, file_system);
	return obj;
}

static JSValue nx_fs_free_space(JSContext *ctx, JSValueConst this_val, int argc,
								JSValueConst *argv) {
	nx_file_system_t *file_system =
		JS_GetOpaque2(ctx, this_val, nx_file_system_class_id);
	if (!file_system) {
		return JS_EXCEPTION;
	}
	s64 space;
	Result rc = fsFsGetFreeSpace(&file_system->fs, "/", &space);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "fsFsGetFreeSpace()");
	}
	return JS_NewBigInt64(ctx, space);
}

static JSValue nx_fs_total_space(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	nx_file_system_t *file_system =
		JS_GetOpaque2(ctx, this_val, nx_file_system_class_id);
	if (!file_system) {
		return JS_EXCEPTION;
	}
	s64 space;
	Result rc = fsFsGetTotalSpace(&file_system->fs, "/", &space);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "fsFsGetTotalSpace()");
	}
	return JS_NewBigInt64(ctx, space);
}

static JSValue nx_save_data_id(JSContext *ctx, JSValueConst this_val, int argc,
							   JSValueConst *argv) {
	nx_save_data_t *save_data =
		JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data) {
		return JS_EXCEPTION;
	}
	return JS_NewBigUint64(ctx, save_data->info.save_data_id);
}

static JSValue nx_save_data_space_id(JSContext *ctx, JSValueConst this_val,
									 int argc, JSValueConst *argv) {
	nx_save_data_t *save_data =
		JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data) {
		return JS_EXCEPTION;
	}
	return JS_NewUint32(ctx, save_data->info.save_data_space_id);
}

static JSValue nx_save_data_type(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	nx_save_data_t *save_data =
		JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data) {
		return JS_EXCEPTION;
	}
	return JS_NewUint32(ctx, save_data->info.save_data_type);
}

static JSValue nx_save_data_uid(JSContext *ctx, JSValueConst this_val, int argc,
								JSValueConst *argv) {
	nx_save_data_t *save_data =
		JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data) {
		return JS_EXCEPTION;
	}
	JSValue uid = JS_NewArray(ctx);
	JS_SetPropertyUint32(ctx, uid, 0,
						 JS_NewBigUint64(ctx, save_data->info.uid.uid[0]));
	JS_SetPropertyUint32(ctx, uid, 1,
						 JS_NewBigUint64(ctx, save_data->info.uid.uid[1]));
	return uid;
}

static JSValue nx_save_data_system_id(JSContext *ctx, JSValueConst this_val,
									  int argc, JSValueConst *argv) {
	nx_save_data_t *save_data =
		JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data) {
		return JS_EXCEPTION;
	}
	return JS_NewBigUint64(ctx, save_data->info.system_save_data_id);
}

static JSValue nx_save_data_application_id(JSContext *ctx,
										   JSValueConst this_val, int argc,
										   JSValueConst *argv) {
	nx_save_data_t *save_data =
		JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data) {
		return JS_EXCEPTION;
	}
	return JS_NewBigUint64(ctx, save_data->info.application_id);
}

static JSValue nx_save_data_size(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	nx_save_data_t *save_data =
		JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data) {
		return JS_EXCEPTION;
	}
	return JS_NewBigUint64(ctx, save_data->info.size);
}

static JSValue nx_save_data_index(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_save_data_t *save_data =
		JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data) {
		return JS_EXCEPTION;
	}
	return JS_NewUint32(ctx, save_data->info.save_data_index);
}

static JSValue nx_save_data_rank(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	nx_save_data_t *save_data =
		JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data) {
		return JS_EXCEPTION;
	}
	return JS_NewUint32(ctx, save_data->info.save_data_rank);
}

static JSValue nx_save_data_commit(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_save_data_t *save_data =
		JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data) {
		return JS_EXCEPTION;
	}
	if (!save_data->mount_name) {
		return JS_ThrowTypeError(ctx, "SaveData is not mounted");
	}
	Result rc = fsFsCommit(&save_data->fs);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "fsFsCommit()");
	}
	return JS_UNDEFINED;
}

static JSValue nx_save_data_delete(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_save_data_t *save_data =
		JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data) {
		return JS_EXCEPTION;
	}
	Result rc = fsDeleteSaveDataFileSystemBySaveDataSpaceId(
		save_data->info.save_data_space_id, save_data->info.save_data_id);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(
			ctx, rc, "fsDeleteSaveDataFileSystemBySaveDataSpaceId()");
	}
	return JS_UNDEFINED;
}

static JSValue nx_save_data_extend(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	s64 data_size;
	s64 journal_size;
	nx_save_data_t *save_data =
		JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data || JS_ToBigInt64(ctx, &data_size, argv[0]) ||
		JS_ToBigInt64(ctx, &journal_size, argv[1])) {
		return JS_EXCEPTION;
	}
	Result rc = fsExtendSaveDataFileSystem(save_data->info.save_data_space_id,
										   save_data->info.save_data_id,
										   data_size, journal_size);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "fsExtendSaveDataFileSystem()");
	}
	return JS_UNDEFINED;
}

static JSValue nx_save_data_mount(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_save_data_t *save_data =
		JS_GetOpaque2(ctx, argv[0], nx_save_data_class_id);
	if (!save_data) {
		return JS_EXCEPTION;
	}

	if (save_data->mount_name) {
		return JS_ThrowTypeError(ctx, "Save data is already mounted");
	}

	Result rc;
	FsSaveDataAttribute attr = {0};
	switch (save_data->info.save_data_type) {
	case FsSaveDataType_System:
	case FsSaveDataType_SystemBcat: {
		attr.uid = save_data->info.uid;
		attr.system_save_data_id = save_data->info.system_save_data_id;
		attr.save_data_type = save_data->info.save_data_type;
		rc = fsOpenSaveDataFileSystemBySystemSaveDataId(
			&save_data->fs,
			(FsSaveDataSpaceId)save_data->info.save_data_space_id, &attr);
	} break;

	case FsSaveDataType_Account: {
		attr.uid = save_data->info.uid;
		attr.application_id = save_data->info.application_id;
		attr.save_data_type = save_data->info.save_data_type;
		attr.save_data_rank = save_data->info.save_data_rank;
		attr.save_data_index = save_data->info.save_data_index;
		rc = fsOpenSaveDataFileSystem(
			&save_data->fs,
			(FsSaveDataSpaceId)save_data->info.save_data_space_id, &attr);
	} break;

	case FsSaveDataType_Device: {
		attr.application_id = save_data->info.application_id;
		attr.save_data_type = FsSaveDataType_Device;
		rc = fsOpenSaveDataFileSystem(
			&save_data->fs,
			(FsSaveDataSpaceId)save_data->info.save_data_space_id, &attr);
	} break;

	case FsSaveDataType_Bcat: {
		attr.application_id = save_data->info.application_id;
		attr.save_data_type = FsSaveDataType_Bcat;
		rc = fsOpenSaveDataFileSystem(
			&save_data->fs,
			(FsSaveDataSpaceId)save_data->info.save_data_space_id, &attr);
	} break;

	case FsSaveDataType_Cache: {
		attr.application_id = save_data->info.application_id;
		attr.save_data_type = FsSaveDataType_Cache;
		attr.save_data_index = save_data->info.save_data_index;
		rc = fsOpenSaveDataFileSystem(
			&save_data->fs,
			(FsSaveDataSpaceId)save_data->info.save_data_space_id, &attr);
	} break;

	case FsSaveDataType_Temporary: {
		attr.application_id = save_data->info.application_id;
		attr.save_data_type = save_data->info.save_data_type;
		rc = fsOpenSaveDataFileSystem(
			&save_data->fs,
			(FsSaveDataSpaceId)save_data->info.save_data_space_id, &attr);
	} break;

	default:
		rc = 1;
		break;
	}

	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "fsOpenSaveDataFileSystem()");
	}

	const char *name = JS_ToCString(ctx, argv[1]);
	if (!name) {
		return JS_EXCEPTION;
	}

	int ret = fsdevMountDevice(name, save_data->fs);
	if (ret == -1)
		rc = MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);

	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "fsdevMountDevice()");
	}

	save_data->mount_name = js_strdup(ctx, name);
	JS_FreeCString(ctx, name);
	return JS_UNDEFINED;
}

static JSValue nx_save_data_unmount(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_save_data_t *save_data =
		JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data) {
		return JS_EXCEPTION;
	}
	if (!save_data->mount_name) {
		return JS_ThrowTypeError(ctx, "SaveData is not mounted");
	}
	Result rc = fsdevUnmountDevice(save_data->mount_name);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "fsdevUnmountDevice()");
	}
	js_free(ctx, (void *)save_data->mount_name);
	save_data->mount_name = NULL;
	JS_SetPropertyStr(ctx, this_val, "url", JS_NULL);
	return JS_UNDEFINED;
}

static JSValue nx_save_data_free_space(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	nx_save_data_t *save_data =
		JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data) {
		return JS_EXCEPTION;
	}
	if (!save_data->mount_name) {
		return JS_ThrowTypeError(ctx, "SaveData is not mounted");
	}
	s64 space;
	Result rc = fsFsGetFreeSpace(&save_data->fs, "/", &space);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "fsFsGetFreeSpace()");
	}
	return JS_NewBigInt64(ctx, space);
}

static JSValue nx_save_data_total_space(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	nx_save_data_t *save_data =
		JS_GetOpaque2(ctx, this_val, nx_save_data_class_id);
	if (!save_data) {
		return JS_EXCEPTION;
	}
	if (!save_data->mount_name) {
		return JS_ThrowTypeError(ctx, "SaveData is not mounted");
	}
	s64 space;
	Result rc = fsFsGetTotalSpace(&save_data->fs, "/", &space);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "fsFsGetTotalSpace()");
	}
	return JS_NewBigInt64(ctx, space);
}

static JSValue nx_fs_open_save_data_info_reader(JSContext *ctx,
												JSValueConst this_val, int argc,
												JSValueConst *argv) {
	u32 space_id;
	if (JS_ToUint32(ctx, &space_id, argv[0])) {
		return JS_EXCEPTION;
	}
	nx_save_data_iterator_t *save_data_iterator =
		js_mallocz(ctx, sizeof(nx_save_data_iterator_t));
	if (!save_data_iterator) {
		return JS_EXCEPTION;
	}
	Result rc = fsOpenSaveDataInfoReader(&save_data_iterator->it,
										 (FsSaveDataSpaceId)space_id);
	if (R_FAILED(rc)) {
		return JS_NULL;
	}
	JSValue it = JS_NewObjectClass(ctx, nx_save_data_iterator_class_id);
	JS_SetOpaque(it, save_data_iterator);
	return it;
}

static JSValue nx_fs_save_data_info_reader_next(JSContext *ctx,
												JSValueConst this_val, int argc,
												JSValueConst *argv) {
	nx_save_data_iterator_t *save_data_iterator =
		JS_GetOpaque2(ctx, argv[0], nx_save_data_iterator_class_id);
	if (!save_data_iterator) {
		return JS_EXCEPTION;
	}
	nx_save_data_t *save_data = js_mallocz(ctx, sizeof(nx_save_data_t));
	if (!save_data) {
		return JS_EXCEPTION;
	}
	s64 total = 0;
	Result rc = fsSaveDataInfoReaderRead(&save_data_iterator->it,
										 &save_data->info, 1, &total);
	if (R_FAILED(rc)) {
		js_free(ctx, save_data);
		return nx_throw_libnx_error(ctx, rc, "fsSaveDataInfoReaderRead()");
	}
	if (total == 0) {
		js_free(ctx, save_data);
		return JS_NULL;
	}
	JSValue obj = JS_NewObjectClass(ctx, nx_save_data_class_id);
	JS_SetOpaque(obj, save_data);
	JS_SetPropertyStr(ctx, obj, "url", JS_NULL);
	return obj;
}

static JSValue nx_save_data_create_sync(JSContext *ctx, JSValueConst this_val,
										int argc, JSValueConst *argv) {
	JSValue val;
	FsSaveDataAttribute attr = {0};
	FsSaveDataCreationInfo crt = {0};
	FsSaveDataMetaInfo meta = {0};

	val = JS_GetPropertyStr(ctx, argv[0], "type");
	if (JS_IsNumber(val)) {
		if (JS_ToUint32(ctx, (u32 *)&attr.save_data_type, val)) {
			return JS_EXCEPTION;
		}
	}
	JS_FreeValue(ctx, val);

	if (argc >= 2 && !JS_IsUndefined(argv[1])) {
		size_t nacp_size;
		NacpStruct *nacp =
			(NacpStruct *)JS_GetArrayBuffer(ctx, &nacp_size, argv[1]);
		if (nacp_size != sizeof(NacpStruct)) {
			return JS_ThrowTypeError(
				ctx, "Invalid NACP buffer (got %ld bytes, expected %ld)",
				nacp_size, sizeof(NacpStruct));
		}
		attr.application_id = nacp->save_data_owner_id;
		switch (attr.save_data_type) {
		case FsSaveDataType_Account:
			crt.save_data_size = nacp->user_account_save_data_size;
			crt.journal_size = nacp->user_account_save_data_journal_size;
			break;

		case FsSaveDataType_Device:
			crt.save_data_size = nacp->device_save_data_size;
			crt.journal_size = nacp->device_save_data_journal_size;
			break;

		case FsSaveDataType_Bcat:
			crt.save_data_size = nacp->bcat_delivery_cache_storage_size;
			crt.journal_size = nacp->bcat_delivery_cache_storage_size;
			break;

		case FsSaveDataType_Cache:
			crt.save_data_size = nacp->cache_storage_size;
			if (nacp->cache_storage_journal_size >
				nacp->cache_storage_data_and_journal_size_max)
				crt.journal_size = nacp->cache_storage_journal_size;
			else
				crt.journal_size =
					nacp->cache_storage_data_and_journal_size_max;
			break;
		}
	}

	val = JS_GetPropertyStr(ctx, argv[0], "spaceId");
	if (JS_IsNumber(val)) {
		if (JS_ToUint32(ctx, (u32 *)&crt.save_data_space_id, val)) {
			return JS_EXCEPTION;
		}
	}
	JS_FreeValue(ctx, val);

	val = JS_GetPropertyStr(ctx, argv[0], "size");
	if (JS_IsBigInt(ctx, val)) {
		if (JS_ToBigInt64(ctx, &crt.save_data_size, val)) {
			return JS_EXCEPTION;
		}
	}
	JS_FreeValue(ctx, val);

	val = JS_GetPropertyStr(ctx, argv[0], "journalSize");
	if (JS_IsBigInt(ctx, val)) {
		if (JS_ToBigInt64(ctx, &crt.journal_size, val)) {
			return JS_EXCEPTION;
		}
	}
	JS_FreeValue(ctx, val);

	val = JS_GetPropertyStr(ctx, argv[0], "uid");
	if (JS_IsArray(val)) {
		if (JS_ToBigUint64(ctx, &attr.uid.uid[0],
						   JS_GetPropertyUint32(ctx, val, 0)) ||
			JS_ToBigUint64(ctx, &attr.uid.uid[1],
						   JS_GetPropertyUint32(ctx, val, 1))) {
			return JS_EXCEPTION;
		}
	}
	JS_FreeValue(ctx, val);

	val = JS_GetPropertyStr(ctx, argv[0], "systemId");
	if (JS_IsBigInt(ctx, val)) {
		if (JS_ToBigUint64(ctx, &attr.system_save_data_id, val)) {
			return JS_EXCEPTION;
		}
	}
	JS_FreeValue(ctx, val);

	val = JS_GetPropertyStr(ctx, argv[0], "applicationId");
	if (JS_IsBigInt(ctx, val)) {
		if (JS_ToBigUint64(ctx, &attr.application_id, val)) {
			return JS_EXCEPTION;
		}
	}
	JS_FreeValue(ctx, val);

	val = JS_GetPropertyStr(ctx, argv[0], "index");
	if (JS_IsNumber(val)) {
		if (JS_ToUint32(ctx, (u32 *)&attr.save_data_index, val)) {
			return JS_EXCEPTION;
		}
	}
	JS_FreeValue(ctx, val);

	val = JS_GetPropertyStr(ctx, argv[0], "rank");
	if (JS_IsNumber(val)) {
		if (JS_ToUint32(ctx, (u32 *)&attr.save_data_rank, val)) {
			return JS_EXCEPTION;
		}
	}
	JS_FreeValue(ctx, val);

	// TODO: make configurable?
	crt.available_size = 0x4000;
	crt.flags = 0;
	crt.owner_id = attr.save_data_type == FsSaveDataType_Bcat
					   ? 0x010000000000000C
					   : attr.application_id;

	// TODO: make configurable?
	if (attr.save_data_type != FsSaveDataType_Bcat) {
		meta.size = 0x40060;
		meta.type = FsSaveDataMetaType_Thumbnail;
	}

	Result rc = fsCreateSaveDataFileSystem(&attr, &crt, &meta);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "fsCreateSaveDataFileSystem()");
	}

	return JS_UNDEFINED;
}

static JSValue nx_fs_init(JSContext *ctx, JSValueConst this_val, int argc,
						  JSValueConst *argv) {
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_FUNC(proto, "freeSpace", nx_fs_free_space, 0);
	NX_DEF_FUNC(proto, "totalSpace", nx_fs_total_space, 0);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static JSValue nx_save_data_init(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
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
	NX_DEF_FUNC(proto, "freeSpace", nx_save_data_free_space, 0);
	NX_DEF_FUNC(proto, "totalSpace", nx_save_data_total_space, 0);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("fsInit", 1, nx_fs_init),
	JS_CFUNC_DEF("fsMount", 1, nx_fs_mount),
	JS_CFUNC_DEF("fsOpenBis", 1, nx_fs_open_bis),
	JS_CFUNC_DEF("fsOpenSdmc", 1, nx_fs_open_sdmc),
	JS_CFUNC_DEF("fsOpenWithId", 1, nx_fs_open_with_id),

	JS_CFUNC_DEF("saveDataInit", 1, nx_save_data_init),
	JS_CFUNC_DEF("saveDataMount", 1, nx_save_data_mount),
	JS_CFUNC_DEF("saveDataCreateSync", 1, nx_save_data_create_sync),

	JS_CFUNC_DEF("fsOpenSaveDataInfoReader", 1,
				 nx_fs_open_save_data_info_reader),
	JS_CFUNC_DEF("fsSaveDataInfoReaderNext", 1,
				 nx_fs_save_data_info_reader_next),
};

void nx_init_fsdev(JSContext *ctx, JSValueConst init_obj) {
	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_file_system_class_id);
	JSClassDef file_system_class = {
		"FileSystem",
		.finalizer = finalizer_file_system,
	};
	JS_NewClass(rt, nx_file_system_class_id, &file_system_class);

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

	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
