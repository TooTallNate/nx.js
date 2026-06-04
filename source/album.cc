#include "async.h"
#include "error.h"
#include "types.h"
#include "wrap.h"
#include <stdio.h>
#include <time.h>

using namespace v8;

namespace {

typedef struct {
	CapsAlbumEntry entry;
} nx_album_file_t;

void nx_capsa_exit(const FunctionCallbackInfo<Value> &info) { capsaExit(); }

void nx_capsa_initialize(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Result rc = capsaInitialize();
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "capsaInitialize");
		return;
	}
	Local<Context> context = iso->GetCurrentContext();
	info.GetReturnValue().Set(FunctionTemplate::New(iso, nx_capsa_exit)
	                              ->GetFunction(context)
	                              .ToLocalChecked());
}

// Read the `storage` property off an object as a CapsAlbumStorage.
bool get_storage(Isolate *iso, Local<Object> obj, CapsAlbumStorage *out) {
	Local<Context> context = iso->GetCurrentContext();
	Local<Value> v;
	if (!obj->Get(context, nx_str(iso, "storage")).ToLocal(&v)) {
		return false;
	}
	uint32_t s = 0;
	if (!v->Uint32Value(context).To(&s)) {
		return false;
	}
	*out = (CapsAlbumStorage)s;
	return true;
}

void nx_album_size(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	CapsAlbumStorage storage;
	if (!get_storage(iso, info.This().As<Object>(), &storage))
		return;
	u64 count;
	Result rc = capsaGetAlbumFileCount(storage, &count);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "capsaGetAlbumFileCount");
		return;
	}
	info.GetReturnValue().Set(Integer::NewFromUnsigned(iso, (u32)count));
}

void nx_album_delete_file(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	CapsAlbumStorage storage;
	if (!get_storage(iso, info.This().As<Object>(), &storage))
		return;
	nx_album_file_t *file = nx::Unwrap<nx_album_file_t>(info[0]);
	if (!file)
		return;
	if (file->entry.file_id.storage != storage) {
		info.GetReturnValue().Set(false);
		return;
	}
	Result rc = capsaDeleteAlbumFile(&file->entry.file_id);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "capsaDeleteAlbumFile");
		return;
	}
	info.GetReturnValue().Set(true);
}

Local<String> entry_id_to_name(Isolate *iso, CapsAlbumFileId *id) {
	char name[54];
	snprintf(name, sizeof(name), "%04d%02d%02d%02d%02d%02d%02d-%016lX.%s",
	         id->datetime.year, id->datetime.month, id->datetime.day,
	         id->datetime.hour, id->datetime.minute, id->datetime.second,
	         id->datetime.id, id->application_id,
	         id->content == 0 ? "jpg" : "mp4");
	return nx_str(iso, name);
}

Local<Value> entry_id_to_date(Isolate *iso, CapsAlbumFileId *id) {
	struct tm t = {0};
	t.tm_year = id->datetime.year - 1900;
	t.tm_mon = id->datetime.month - 1;
	t.tm_mday = id->datetime.day;
	t.tm_hour = id->datetime.hour;
	t.tm_min = id->datetime.minute;
	t.tm_sec = id->datetime.second;
	t.tm_isdst = -1;
	Local<Context> context = iso->GetCurrentContext();
	return Date::New(context, mktime(&t) * 1000.0).ToLocalChecked();
}

void nx_album_file_list(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	CapsAlbumStorage storage;
	if (!get_storage(iso, info[0].As<Object>(), &storage))
		return;
	u64 count;
	Result rc = capsaGetAlbumFileCount(storage, &count);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "capsaGetAlbumFileCount");
		return;
	}
	CapsAlbumEntry *entry =
	    (CapsAlbumEntry *)nx_alloc(iso, sizeof(CapsAlbumEntry) * count);
	if (!entry)
		return;
	u64 out;
	rc = capsaGetAlbumFileList(CapsAlbumStorage_Sd, &out, entry, count);
	if (R_FAILED(rc)) {
		free(entry);
		nx_throw_libnx_error(iso, rc, "capsaGetAlbumFileList");
		return;
	}
	Local<Array> arr = Array::New(iso, out);
	for (u64 i = 0; i < out; i++) {
		nx_album_file_t *d =
		    static_cast<nx_album_file_t *>(calloc(1, sizeof(nx_album_file_t)));
		d->entry = entry[i];
		Local<Object> entry_val = nx::NewWrapped(iso);
		nx::Wrap<nx_album_file_t>(iso, entry_val, d,
		                          [](nx_album_file_t *p) { free(p); });
		entry_val
		    ->Set(context, nx_str(iso, "name"),
		          entry_id_to_name(iso, &d->entry.file_id))
		    .Check();
		entry_val
		    ->Set(context, nx_str(iso, "lastModified"),
		          entry_id_to_date(iso, &d->entry.file_id))
		    .Check();
		arr->Set(context, (uint32_t)i, entry_val).Check();
	}
	free(entry);
	info.GetReturnValue().Set(arr);
}

void nx_album_file_type(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_album_file_t *file = nx::Unwrap<nx_album_file_t>(info.This());
	if (!file)
		return;
	bool is_image =
	    file->entry.file_id.content == CapsAlbumFileContents_ScreenShot ||
	    file->entry.file_id.content == CapsAlbumFileContents_ExtraScreenShot;
	info.GetReturnValue().Set(
	    nx_str(iso, is_image ? "image/jpeg" : "video/mp4"));
}

void nx_album_file_size(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_album_file_t *file = nx::Unwrap<nx_album_file_t>(info.This());
	if (!file)
		return;
	u64 size;
	Result rc = capsaGetAlbumFileSize(&file->entry.file_id, &size);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "capsaGetAlbumFileSize");
		return;
	}
	info.GetReturnValue().Set(BigInt::NewFromUnsigned(iso, size));
}

void nx_album_file_app_id(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_album_file_t *file = nx::Unwrap<nx_album_file_t>(info.This());
	if (!file)
		return;
	info.GetReturnValue().Set(
	    BigInt::NewFromUnsigned(iso, file->entry.file_id.application_id));
}

void nx_album_file_content(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_album_file_t *file = nx::Unwrap<nx_album_file_t>(info.This());
	if (!file)
		return;
	info.GetReturnValue().Set(
	    Integer::NewFromUnsigned(iso, file->entry.file_id.content));
}

void nx_album_file_storage(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_album_file_t *file = nx::Unwrap<nx_album_file_t>(info.This());
	if (!file)
		return;
	info.GetReturnValue().Set(
	    Integer::NewFromUnsigned(iso, file->entry.file_id.storage));
}

void nx_album_file_id(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_album_file_t *file = nx::Unwrap<nx_album_file_t>(info.This());
	if (!file)
		return;
	info.GetReturnValue().Set(
	    Integer::NewFromUnsigned(iso, file->entry.file_id.datetime.id));
}

// ---- async: thumbnail() and arrayBuffer() ----
typedef struct {
	CapsAlbumFileId id;
	int err;
	uint8_t *data;
	u64 size;
} album_async_t;

void thumbnail_work(nx_work_t *req) {
	capsaInitialize();
	album_async_t *d = (album_async_t *)req->data;
	u64 buf_size = 100 * 1024;
	d->data = (uint8_t *)malloc(buf_size);
	if (!d->data) {
		// Worker thread: cannot touch V8. Signal failure via err; the
		// after-work callback turns R_FAILED(err) into a thrown JS error.
		d->err = MAKERESULT(Module_Libnx, LibnxError_HeapAllocFailed);
		return;
	}
	d->err =
	    capsaLoadAlbumFileThumbnail(&d->id, &d->size, d->data, buf_size);
}

void array_buffer_work(nx_work_t *req) {
	capsaInitialize();
	album_async_t *d = (album_async_t *)req->data;
	u64 size;
	d->err = capsaGetAlbumFileSize(&d->id, &size);
	if (R_SUCCEEDED(d->err)) {
		d->data = (uint8_t *)malloc(size);
		if (!d->data) {
			d->err = MAKERESULT(Module_Libnx, LibnxError_HeapAllocFailed);
			return;
		}
		d->err = capsaLoadAlbumFile(&d->id, &d->size, d->data, size);
	}
}

MaybeLocal<Value> album_buffer_after(Isolate *iso, nx_work_t *req) {
	album_async_t *d = (album_async_t *)req->data;
	if (R_FAILED(d->err)) {
		if (d->data)
			free(d->data);
		d->data = nullptr;
		nx_throw_libnx_error(iso, d->err, "capsaLoadAlbumFile");
		return MaybeLocal<Value>();
	}
	// Transfer ownership of d->data to the ArrayBuffer; null it so the
	// framework's free(req->data) doesn't double-free the payload struct only.
	uint8_t *buf = d->data;
	u64 size = d->size;
	d->data = nullptr;
	std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
	    buf, size, [](void *p, size_t, void *) { free(p); }, nullptr);
	return ArrayBuffer::New(iso, std::move(bs));
}

void nx_album_file_thumbnail(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_album_file_t *file = nx::Unwrap<nx_album_file_t>(info.This());
	if (!file)
		return;
	NX_INIT_WORK_T(album_async_t);
	data->id = file->entry.file_id;
	info.GetReturnValue().Set(
	    nx_queue_async(iso, req, thumbnail_work, album_buffer_after));
}

void nx_album_file_array_buffer(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_album_file_t *file = nx::Unwrap<nx_album_file_t>(info.This());
	if (!file)
		return;
	NX_INIT_WORK_T(album_async_t);
	data->id = file->entry.file_id;
	info.GetReturnValue().Set(
	    nx_queue_async(iso, req, array_buffer_work, album_buffer_after));
}

Local<Object> get_proto(Isolate *iso, const FunctionCallbackInfo<Value> &info) {
	Local<Context> context = iso->GetCurrentContext();
	return info[0]
	    .As<Object>()
	    ->Get(context, nx_str(iso, "prototype"))
	    .ToLocalChecked()
	    .As<Object>();
}

void nx_album_init(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Object> proto = get_proto(iso, info);
	NX_DEF_GET(proto, "size", nx_album_size);
	NX_DEF_FUNC(proto, "delete", nx_album_delete_file, 1);
}

void nx_album_file_init(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Object> proto = get_proto(iso, info);
	NX_DEF_GET(proto, "type", nx_album_file_type);
	NX_DEF_GET(proto, "size", nx_album_file_size);
	NX_DEF_GET(proto, "applicationId", nx_album_file_app_id);
	NX_DEF_GET(proto, "content", nx_album_file_content);
	NX_DEF_GET(proto, "storage", nx_album_file_storage);
	NX_DEF_GET(proto, "id", nx_album_file_id);
	NX_DEF_FUNC(proto, "arrayBuffer", nx_album_file_array_buffer, 0);
	NX_DEF_FUNC(proto, "thumbnail", nx_album_file_thumbnail, 0);
}

} // namespace

void nx_init_album(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "capsaInitialize", nx_capsa_initialize);
	NX_SET_FUNC(init_obj, "albumInit", nx_album_init);
	NX_SET_FUNC(init_obj, "albumFileInit", nx_album_file_init);
	NX_SET_FUNC(init_obj, "albumFileList", nx_album_file_list);
}
