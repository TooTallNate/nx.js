#include "error.h"
#include "types.h"
#include "util.h"
#include "wrap.h"
#include <string.h>

using namespace v8;

namespace {

typedef struct {
	FsFileSystem fs;
	char mount_name[32];
} nx_file_system_t;

typedef struct {
	bool info_loaded;
	FsSaveDataInfo info;
	FsFileSystem fs;
	char *mount_name;
} nx_save_data_t;

typedef struct {
	FsSaveDataInfoReader it;
} nx_save_data_iterator_t;

void free_file_system(nx_file_system_t *fs) {
	if (strlen(fs->mount_name) > 0) {
		fsdevUnmountDevice(fs->mount_name);
		fsFsClose(&fs->fs);
	}
	free(fs);
}
void free_save_data(nx_save_data_t *sd) {
	if (sd->mount_name) {
		fsdevUnmountDevice(sd->mount_name);
		free(sd->mount_name);
		fsFsClose(&sd->fs);
	}
	free(sd);
}
void free_save_data_iterator(nx_save_data_iterator_t *it) {
	fsSaveDataInfoReaderClose(&it->it);
	free(it);
}

nx_file_system_t *get_fs(Local<Value> v) {
	return nx::Unwrap<nx_file_system_t>(v);
}
nx_save_data_t *get_sd(Local<Value> v) { return nx::Unwrap<nx_save_data_t>(v); }

Local<Object> wrap_fs(Isolate *iso, nx_file_system_t *fs) {
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_file_system_t>(iso, obj, fs, free_file_system);
	return obj;
}

// ---- FileSystem ----
void nx_fs_mount(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_file_system_t *fs = get_fs(info[0]);
	if (!fs)
		return;
	String::Utf8Value name(iso, info[1]);
	if (!*name)
		return;
	int r = fsdevMountDevice(*name, fs->fs);
	if (r < 0) {
		nx_throw_libnx_error(iso, r, "fsdevMountDevice");
		return;
	}
	strncpy(fs->mount_name, *name, sizeof(fs->mount_name) - 1);
}

void nx_fs_open_bis(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	uint32_t id;
	if (!info[0]->Uint32Value(iso->GetCurrentContext()).To(&id))
		return;
	nx_file_system_t *fs =
	    (nx_file_system_t *)calloc(1, sizeof(nx_file_system_t));
	Result rc = fsOpenBisFileSystem(&fs->fs, (FsBisPartitionId)id, "/");
	if (R_FAILED(rc)) {
		free(fs);
		nx_throw_libnx_error(iso, rc, "fsOpenBisFileSystem");
		return;
	}
	info.GetReturnValue().Set(wrap_fs(iso, fs));
}

void nx_fs_open_sdmc(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_file_system_t *fs =
	    (nx_file_system_t *)calloc(1, sizeof(nx_file_system_t));
	Result rc = fsOpenSdCardFileSystem(&fs->fs);
	if (R_FAILED(rc)) {
		free(fs);
		nx_throw_libnx_error(iso, rc, "fsOpenSdCardFileSystem");
		return;
	}
	info.GetReturnValue().Set(wrap_fs(iso, fs));
}

void nx_fs_open_with_id(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> c = iso->GetCurrentContext();
	if (!info[0]->IsBigInt()) {
		nx_throw(iso, "expected BigInt titleId");
		return;
	}
	u64 title_id = info[0].As<BigInt>()->Uint64Value();
	uint32_t type = 0, attributes = 0;
	if (!info[1]->Uint32Value(c).To(&type) ||
	    !info[3]->Uint32Value(c).To(&attributes))
		return;
	String::Utf8Value path(iso, info[2]);
	if (!*path)
		return;
	nx_file_system_t *fs =
	    (nx_file_system_t *)calloc(1, sizeof(nx_file_system_t));
	Result rc = fsOpenFileSystemWithId(&fs->fs, title_id,
	                                   (FsFileSystemType)type, *path,
	                                   (FsContentAttributes)attributes);
	if (R_FAILED(rc)) {
		free(fs);
		nx_throw_libnx_error(iso, rc, "fsOpenFileSystemWithId");
		return;
	}
	info.GetReturnValue().Set(wrap_fs(iso, fs));
}

void nx_fs_free_space(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_file_system_t *fs = get_fs(info.This());
	if (!fs)
		return;
	s64 space;
	Result rc = fsFsGetFreeSpace(&fs->fs, "/", &space);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "fsFsGetFreeSpace");
		return;
	}
	info.GetReturnValue().Set(BigInt::New(iso, space));
}
void nx_fs_total_space(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_file_system_t *fs = get_fs(info.This());
	if (!fs)
		return;
	s64 space;
	Result rc = fsFsGetTotalSpace(&fs->fs, "/", &space);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "fsFsGetTotalSpace");
		return;
	}
	info.GetReturnValue().Set(BigInt::New(iso, space));
}

// ---- SaveData getters ----
#define SD_GET_U64(NAME, FIELD)                                                \
	void NAME(const FunctionCallbackInfo<Value> &info) {                       \
		Isolate *iso = info.GetIsolate();                                      \
		nx_save_data_t *sd = get_sd(info.This());                              \
		if (sd)                                                                \
			info.GetReturnValue().Set(                                         \
			    BigInt::NewFromUnsigned(iso, sd->info.FIELD));                 \
	}
#define SD_GET_U32(NAME, FIELD)                                                \
	void NAME(const FunctionCallbackInfo<Value> &info) {                       \
		Isolate *iso = info.GetIsolate();                                      \
		nx_save_data_t *sd = get_sd(info.This());                              \
		if (sd)                                                                \
			info.GetReturnValue().Set(                                         \
			    Integer::NewFromUnsigned(iso, sd->info.FIELD));                \
	}

SD_GET_U64(nx_sd_id, save_data_id)
SD_GET_U32(nx_sd_space_id, save_data_space_id)
SD_GET_U32(nx_sd_type, save_data_type)
SD_GET_U64(nx_sd_system_id, system_save_data_id)
SD_GET_U64(nx_sd_application_id, application_id)
SD_GET_U64(nx_sd_size, size)
SD_GET_U32(nx_sd_index, save_data_index)
SD_GET_U32(nx_sd_rank, save_data_rank)

void nx_sd_uid(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> c = iso->GetCurrentContext();
	nx_save_data_t *sd = get_sd(info.This());
	if (!sd)
		return;
	Local<Array> uid = Array::New(iso, 2);
	uid->Set(c, 0, BigInt::NewFromUnsigned(iso, sd->info.uid.uid[0])).Check();
	uid->Set(c, 1, BigInt::NewFromUnsigned(iso, sd->info.uid.uid[1])).Check();
	info.GetReturnValue().Set(uid);
}

void nx_sd_commit(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_save_data_t *sd = get_sd(info.This());
	if (!sd)
		return;
	if (!sd->mount_name) {
		nx_throw(iso, "SaveData is not mounted");
		return;
	}
	Result rc = fsFsCommit(&sd->fs);
	if (R_FAILED(rc))
		nx_throw_libnx_error(iso, rc, "fsFsCommit");
}

void nx_sd_delete(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_save_data_t *sd = get_sd(info.This());
	if (!sd)
		return;
	Result rc = fsDeleteSaveDataFileSystemBySaveDataSpaceId(
	    (FsSaveDataSpaceId)sd->info.save_data_space_id, sd->info.save_data_id);
	if (R_FAILED(rc))
		nx_throw_libnx_error(iso, rc,
		                     "fsDeleteSaveDataFileSystemBySaveDataSpaceId");
}

void nx_sd_extend(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_save_data_t *sd = get_sd(info.This());
	if (!sd || !info[0]->IsBigInt() || !info[1]->IsBigInt())
		return;
	s64 data_size = info[0].As<BigInt>()->Int64Value();
	s64 journal_size = info[1].As<BigInt>()->Int64Value();
	Result rc = fsExtendSaveDataFileSystem(
	    (FsSaveDataSpaceId)sd->info.save_data_space_id, sd->info.save_data_id,
	    data_size, journal_size);
	if (R_FAILED(rc))
		nx_throw_libnx_error(iso, rc, "fsExtendSaveDataFileSystem");
}

void nx_sd_mount(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_save_data_t *sd = get_sd(info[0]);
	if (!sd)
		return;
	if (sd->mount_name) {
		nx_throw(iso, "Save data is already mounted");
		return;
	}
	Result rc;
	FsSaveDataAttribute attr = {};
	switch (sd->info.save_data_type) {
	case FsSaveDataType_System:
	case FsSaveDataType_SystemBcat:
		attr.uid = sd->info.uid;
		attr.system_save_data_id = sd->info.system_save_data_id;
		attr.save_data_type = sd->info.save_data_type;
		rc = fsOpenSaveDataFileSystemBySystemSaveDataId(
		    &sd->fs, (FsSaveDataSpaceId)sd->info.save_data_space_id, &attr);
		break;
	case FsSaveDataType_Account:
		attr.uid = sd->info.uid;
		attr.application_id = sd->info.application_id;
		attr.save_data_type = sd->info.save_data_type;
		attr.save_data_rank = sd->info.save_data_rank;
		attr.save_data_index = sd->info.save_data_index;
		rc = fsOpenSaveDataFileSystem(
		    &sd->fs, (FsSaveDataSpaceId)sd->info.save_data_space_id, &attr);
		break;
	case FsSaveDataType_Device:
		attr.application_id = sd->info.application_id;
		attr.save_data_type = FsSaveDataType_Device;
		rc = fsOpenSaveDataFileSystem(
		    &sd->fs, (FsSaveDataSpaceId)sd->info.save_data_space_id, &attr);
		break;
	case FsSaveDataType_Bcat:
		attr.application_id = sd->info.application_id;
		attr.save_data_type = FsSaveDataType_Bcat;
		rc = fsOpenSaveDataFileSystem(
		    &sd->fs, (FsSaveDataSpaceId)sd->info.save_data_space_id, &attr);
		break;
	case FsSaveDataType_Cache:
		attr.application_id = sd->info.application_id;
		attr.save_data_type = FsSaveDataType_Cache;
		attr.save_data_index = sd->info.save_data_index;
		rc = fsOpenSaveDataFileSystem(
		    &sd->fs, (FsSaveDataSpaceId)sd->info.save_data_space_id, &attr);
		break;
	case FsSaveDataType_Temporary:
		attr.application_id = sd->info.application_id;
		attr.save_data_type = sd->info.save_data_type;
		rc = fsOpenSaveDataFileSystem(
		    &sd->fs, (FsSaveDataSpaceId)sd->info.save_data_space_id, &attr);
		break;
	default:
		rc = 1;
		break;
	}
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "fsOpenSaveDataFileSystem");
		return;
	}
	String::Utf8Value name(iso, info[1]);
	if (!*name)
		return;
	int ret = fsdevMountDevice(*name, sd->fs);
	if (ret == -1) {
		nx_throw_libnx_error(iso, MAKERESULT(Module_Libnx, LibnxError_OutOfMemory),
		                     "fsdevMountDevice");
		return;
	}
	sd->mount_name = strdup(*name);
}

void nx_sd_unmount(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> c = iso->GetCurrentContext();
	nx_save_data_t *sd = get_sd(info.This());
	if (!sd)
		return;
	if (!sd->mount_name) {
		nx_throw(iso, "SaveData is not mounted");
		return;
	}
	Result rc = fsdevUnmountDevice(sd->mount_name);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "fsdevUnmountDevice");
		return;
	}
	free(sd->mount_name);
	sd->mount_name = NULL;
	info.This().As<Object>()->Set(c, nx_str(iso, "url"), Null(iso)).Check();
}

void nx_sd_free_space(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_save_data_t *sd = get_sd(info.This());
	if (!sd || !sd->mount_name) {
		nx_throw(iso, "SaveData is not mounted");
		return;
	}
	s64 space;
	Result rc = fsFsGetFreeSpace(&sd->fs, "/", &space);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "fsFsGetFreeSpace");
		return;
	}
	info.GetReturnValue().Set(BigInt::New(iso, space));
}
void nx_sd_total_space(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_save_data_t *sd = get_sd(info.This());
	if (!sd || !sd->mount_name) {
		nx_throw(iso, "SaveData is not mounted");
		return;
	}
	s64 space;
	Result rc = fsFsGetTotalSpace(&sd->fs, "/", &space);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "fsFsGetTotalSpace");
		return;
	}
	info.GetReturnValue().Set(BigInt::New(iso, space));
}

void nx_fs_open_save_data_info_reader(
    const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	uint32_t space_id;
	if (!info[0]->Uint32Value(iso->GetCurrentContext()).To(&space_id))
		return;
	nx_save_data_iterator_t *it =
	    (nx_save_data_iterator_t *)calloc(1, sizeof(nx_save_data_iterator_t));
	Result rc = fsOpenSaveDataInfoReader(&it->it, (FsSaveDataSpaceId)space_id);
	if (R_FAILED(rc)) {
		free(it);
		info.GetReturnValue().SetNull();
		return;
	}
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_save_data_iterator_t>(iso, obj, it, free_save_data_iterator);
	info.GetReturnValue().Set(obj);
}

void nx_fs_save_data_info_reader_next(
    const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> c = iso->GetCurrentContext();
	nx_save_data_iterator_t *it = nx::Unwrap<nx_save_data_iterator_t>(info[0]);
	if (!it)
		return;
	nx_save_data_t *sd = (nx_save_data_t *)calloc(1, sizeof(nx_save_data_t));
	s64 total = 0;
	Result rc = fsSaveDataInfoReaderRead(&it->it, &sd->info, 1, &total);
	if (R_FAILED(rc)) {
		free(sd);
		nx_throw_libnx_error(iso, rc, "fsSaveDataInfoReaderRead");
		return;
	}
	if (total == 0) {
		free(sd);
		info.GetReturnValue().SetNull();
		return;
	}
	Local<Object> obj = nx::NewWrapped(iso);
	nx::Wrap<nx_save_data_t>(iso, obj, sd, free_save_data);
	obj->Set(c, nx_str(iso, "url"), Null(iso)).Check();
	info.GetReturnValue().Set(obj);
}

void nx_save_data_create_sync(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> c = iso->GetCurrentContext();
	Local<Object> o = info[0].As<Object>();
	FsSaveDataAttribute attr = {};
	FsSaveDataCreationInfo crt = {};
	FsSaveDataMetaInfo meta = {};

	auto get_u32 = [&](const char *k, void *out) {
		Local<Value> v;
		if (o->Get(c, nx_str(iso, k)).ToLocal(&v) && v->IsNumber()) {
			uint32_t n;
			if (v->Uint32Value(c).To(&n))
				*(u32 *)out = n;
		}
	};
	auto get_bi = [&](const char *k, s64 *out) {
		Local<Value> v;
		if (o->Get(c, nx_str(iso, k)).ToLocal(&v) && v->IsBigInt())
			*out = v.As<BigInt>()->Int64Value();
	};
	auto get_biu = [&](const char *k, u64 *out) {
		Local<Value> v;
		if (o->Get(c, nx_str(iso, k)).ToLocal(&v) && v->IsBigInt())
			*out = v.As<BigInt>()->Uint64Value();
	};

	get_u32("type", &attr.save_data_type);

	if (info.Length() >= 2 && !info[1]->IsUndefined()) {
		size_t nacp_size = 0;
		NacpStruct *nacp = (NacpStruct *)NX_GetBufferSource(iso, &nacp_size,
		                                                    info[1]);
		if (nacp && nacp_size == sizeof(NacpStruct)) {
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
			default:
				break;
			}
		}
	}
	get_u32("spaceId", &crt.save_data_space_id);
	get_bi("size", &crt.save_data_size);
	get_bi("journalSize", &crt.journal_size);
	{
		Local<Value> v;
		if (o->Get(c, nx_str(iso, "uid")).ToLocal(&v) && v->IsArray()) {
			Local<Object> a = v.As<Object>();
			Local<Value> u0 = a->Get(c, 0).ToLocalChecked();
			Local<Value> u1 = a->Get(c, 1).ToLocalChecked();
			if (u0->IsBigInt())
				attr.uid.uid[0] = u0.As<BigInt>()->Uint64Value();
			if (u1->IsBigInt())
				attr.uid.uid[1] = u1.As<BigInt>()->Uint64Value();
		}
	}
	get_biu("systemId", &attr.system_save_data_id);
	get_biu("applicationId", &attr.application_id);
	get_u32("index", &attr.save_data_index);
	get_u32("rank", &attr.save_data_rank);

	crt.available_size = 0x4000;
	crt.flags = 0;
	crt.owner_id = attr.save_data_type == FsSaveDataType_Bcat
	                   ? 0x010000000000000C
	                   : attr.application_id;
	if (attr.save_data_type != FsSaveDataType_Bcat) {
		meta.size = 0x40060;
		meta.type = FsSaveDataMetaType_Thumbnail;
	}
	Result rc = fsCreateSaveDataFileSystem(&attr, &crt, &meta);
	if (R_FAILED(rc))
		nx_throw_libnx_error(iso, rc, "fsCreateSaveDataFileSystem");
}

Local<Object> proto_of(Isolate *iso, const FunctionCallbackInfo<Value> &info) {
	Local<Context> c = iso->GetCurrentContext();
	return info[0]
	    .As<Object>()
	    ->Get(c, nx_str(iso, "prototype"))
	    .ToLocalChecked()
	    .As<Object>();
}

void nx_fs_init(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Object> proto = proto_of(iso, info);
	NX_DEF_FUNC(proto, "freeSpace", nx_fs_free_space, 0);
	NX_DEF_FUNC(proto, "totalSpace", nx_fs_total_space, 0);
}

void nx_save_data_init(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Object> proto = proto_of(iso, info);
	NX_DEF_GET(proto, "id", nx_sd_id);
	NX_DEF_GET(proto, "spaceId", nx_sd_space_id);
	NX_DEF_GET(proto, "type", nx_sd_type);
	NX_DEF_GET(proto, "uid", nx_sd_uid);
	NX_DEF_GET(proto, "systemId", nx_sd_system_id);
	NX_DEF_GET(proto, "applicationId", nx_sd_application_id);
	NX_DEF_GET(proto, "size", nx_sd_size);
	NX_DEF_GET(proto, "index", nx_sd_index);
	NX_DEF_GET(proto, "rank", nx_sd_rank);
	NX_DEF_FUNC(proto, "commit", nx_sd_commit, 0);
	NX_DEF_FUNC(proto, "delete", nx_sd_delete, 0);
	NX_DEF_FUNC(proto, "extend", nx_sd_extend, 2);
	NX_DEF_FUNC(proto, "unmount", nx_sd_unmount, 0);
	NX_DEF_FUNC(proto, "freeSpace", nx_sd_free_space, 0);
	NX_DEF_FUNC(proto, "totalSpace", nx_sd_total_space, 0);
}

} // namespace

void nx_init_fsdev(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "fsInit", nx_fs_init);
	NX_SET_FUNC(init_obj, "fsMount", nx_fs_mount);
	NX_SET_FUNC(init_obj, "fsOpenBis", nx_fs_open_bis);
	NX_SET_FUNC(init_obj, "fsOpenSdmc", nx_fs_open_sdmc);
	NX_SET_FUNC(init_obj, "fsOpenWithId", nx_fs_open_with_id);
	NX_SET_FUNC(init_obj, "saveDataInit", nx_save_data_init);
	NX_SET_FUNC(init_obj, "saveDataMount", nx_sd_mount);
	NX_SET_FUNC(init_obj, "saveDataCreateSync", nx_save_data_create_sync);
	NX_SET_FUNC(init_obj, "fsOpenSaveDataInfoReader",
	            nx_fs_open_save_data_info_reader);
	NX_SET_FUNC(init_obj, "fsSaveDataInfoReaderNext",
	            nx_fs_save_data_info_reader_next);
}
