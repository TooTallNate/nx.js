#include "album.h"
#include "async.h"
#include "error.h"

static JSClassID nx_album_class_id;
static JSClassID nx_album_file_class_id;

typedef struct
{
	CapsAlbumStorage storage;
} nx_album_t;

typedef struct
{
	CapsAlbumEntry entry;
} nx_album_file_t;

static void finalizer_album(JSRuntime *rt, JSValue val)
{
	nx_album_t *album = JS_GetOpaque(val, nx_album_class_id);
	if (album)
	{
		js_free_rt(rt, album);
	}
}

static void finalizer_album_file(JSRuntime *rt, JSValue val)
{
	nx_album_file_t *file = JS_GetOpaque(val, nx_album_file_class_id);
	if (file)
	{
		js_free_rt(rt, file);
	}
}

static void free_array_buffer(JSRuntime *rt, void *opaque, void *ptr)
{
	free(ptr);
}

static JSValue nx_capsa_exit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	capsaExit();
	return JS_UNDEFINED;
}

static JSValue nx_capsa_initialize(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	Result rc = capsaInitialize();
	if (R_FAILED(rc))
	{
		return nx_throw_libnx_error(ctx, rc, "capsaInitialize()");
	}
	return JS_NewCFunction(ctx, nx_capsa_exit, "", 0);
}

static JSValue nx_album_size(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	CapsAlbumStorage storage;
	if (JS_ToUint32(ctx, (u32 *)&storage, JS_GetPropertyStr(ctx, this_val, "storage")))
	{
		return JS_EXCEPTION;
	}

	u64 count;
	Result rc = capsaGetAlbumFileCount(storage, &count);
	if (R_FAILED(rc))
	{
		return nx_throw_libnx_error(ctx, rc, "capsaGetAlbumFileCount()");
	}
	return JS_NewUint32(ctx, (u32)count);
}

static JSValue nx_album_delete_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	CapsAlbumStorage storage;
	if (JS_ToUint32(ctx, (u32 *)&storage, JS_GetPropertyStr(ctx, this_val, "storage")))
	{
		return JS_EXCEPTION;
	}

	nx_album_file_t *file = JS_GetOpaque2(ctx, argv[0], nx_album_file_class_id);
	if (!file)
	{
		return JS_EXCEPTION;
	}
	if (file->entry.file_id.storage != storage)
	{
		return JS_ThrowReferenceError(ctx, "`AlbumFile` does not belong to this `Album`");
	}

	Result rc = capsaDeleteAlbumFile(&file->entry.file_id);
	if (R_FAILED(rc))
	{
		return nx_throw_libnx_error(ctx, rc, "capsaDeleteAlbumFile()");
	}

	return JS_UNDEFINED;
}

JSValue entry_id_to_name(JSContext *ctx, CapsAlbumFileId *id)
{
	char name[54];
	snprintf(name, sizeof(name), "%04d%02d%02d%02d%02d%02d%02d-%016lX.%s",
			 id->datetime.year,
			 id->datetime.month,
			 id->datetime.day,
			 id->datetime.hour,
			 id->datetime.minute,
			 id->datetime.second,
			 id->datetime.id,
			 id->application_id, // TODO: This part is definitely not correct…
			 id->content == 0 ? "jpg" : "mp4");
	return JS_NewString(ctx, name);
}

JSValue entry_id_to_date(JSContext *ctx, CapsAlbumFileId *id)
{
	struct tm time = {0};
	time.tm_year = id->datetime.year - 1900;
	time.tm_mon = id->datetime.month - 1;
	time.tm_mday = id->datetime.day;
	time.tm_hour = id->datetime.hour;
	time.tm_min = id->datetime.minute;
	time.tm_sec = id->datetime.second;
	time.tm_isdst = -1;
	return JS_NewDate(ctx, mktime(&time) * 1000.);
}

static JSValue nx_album_file_list(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	CapsAlbumStorage storage;
	if (JS_ToUint32(ctx, (u32 *)&storage, JS_GetPropertyStr(ctx, argv[0], "storage")))
	{
		return JS_EXCEPTION;
	}

	u64 count;
	Result rc = capsaGetAlbumFileCount(storage, &count);
	if (R_FAILED(rc))
	{
		return nx_throw_libnx_error(ctx, rc, "capsaGetAlbumFileCount()");
	}

	u64 out;
	CapsAlbumEntry entry[count];
	rc = capsaGetAlbumFileList(CapsAlbumStorage_Sd, &out, entry, count);
	if (R_FAILED(rc))
	{
		return nx_throw_libnx_error(ctx, rc, "capsaGetAlbumFileList()");
	}

	JSValue arr = JS_NewArray(ctx);
	for (u64 i = 0; i < out; i++)
	{
		nx_album_file_t *data = js_mallocz(ctx, sizeof(nx_album_file_t));
		if (!data)
		{
			return JS_EXCEPTION;
		}
		data->entry = entry[i];
		JSValue entry_val = JS_NewObjectClass(ctx, nx_album_file_class_id);
		JS_SetOpaque(entry_val, data);
		JS_SetPropertyStr(ctx, entry_val, "name", entry_id_to_name(ctx, &data->entry.file_id));
		JS_SetPropertyStr(ctx, entry_val, "lastModified", entry_id_to_date(ctx, &data->entry.file_id));
		JS_SetPropertyInt64(ctx, arr, (s64)i, entry_val);
	}

	return arr;
}

static JSValue nx_album_file_type(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_album_file_t *file = JS_GetOpaque2(ctx, this_val, nx_album_file_class_id);
	if (!file)
	{
		return JS_EXCEPTION;
	}
	return JS_NewString(
		ctx,
		file->entry.file_id.content == CapsAlbumFileContents_ScreenShot || file->entry.file_id.content == CapsAlbumFileContents_ExtraScreenShot
			? "image/jpeg"
			: "video/mp4");
}

static JSValue nx_album_file_size(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_album_file_t *file = JS_GetOpaque2(ctx, this_val, nx_album_file_class_id);
	if (!file)
	{
		return JS_EXCEPTION;
	}
	u64 size;
	Result rc = capsaGetAlbumFileSize(&file->entry.file_id, &size);
	if (R_FAILED(rc))
	{
		return nx_throw_libnx_error(ctx, rc, "capsaGetAlbumFileSize()");
	}
	return JS_NewBigUint64(ctx, size);
}

typedef struct
{
	CapsAlbumFileId id;
	int err;
	uint8_t *data;
	u64 size;
} nx_album_file_thumbnail_async_t;

void nx_album_file_thumbnail_do(nx_work_t *req)
{
	capsaInitialize();
	nx_album_file_thumbnail_async_t *data = (nx_album_file_thumbnail_async_t *)req->data;
	u64 buf_size = 100 * 1024;
	data->data = malloc(buf_size);
	data->err = capsaLoadAlbumFileThumbnail(&data->id, &data->size, data->data, buf_size);
}

JSValue nx_album_file_thumbnail_cb(JSContext *ctx, nx_work_t *req)
{
	nx_album_file_thumbnail_async_t *data = (nx_album_file_thumbnail_async_t *)req->data;
	if (R_FAILED(data->err))
	{
		return nx_throw_libnx_error(ctx, data->err, "capsaLoadAlbumFileThumbnail()");
	}
	return JS_NewArrayBuffer(ctx, data->data, data->size, free_array_buffer, NULL, false);
}

static JSValue nx_album_file_thumbnail(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_album_file_t *file = JS_GetOpaque2(ctx, this_val, nx_album_file_class_id);
	if (!file)
	{
		return JS_EXCEPTION;
	}
	NX_INIT_WORK_T(nx_album_file_thumbnail_async_t);
	data->id = file->entry.file_id;
	return nx_queue_async(ctx, req, nx_album_file_thumbnail_do, nx_album_file_thumbnail_cb);
}

static JSValue nx_album_init(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_GET(proto, "size", nx_album_size);
	NX_DEF_FUNC(proto, "delete", nx_album_delete_file, 1);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static JSValue nx_album_file_init(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_GET(proto, "type", nx_album_file_type);
	NX_DEF_GET(proto, "size", nx_album_file_size);
	NX_DEF_FUNC(proto, "thumbnail", nx_album_file_thumbnail, 0);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("capsaInitialize", 0, nx_capsa_initialize),
	JS_CFUNC_DEF("albumInit", 1, nx_album_init),
	JS_CFUNC_DEF("albumFileInit", 1, nx_album_file_init),
	JS_CFUNC_DEF("albumFileList", 1, nx_album_file_list),
};

void nx_init_album(JSContext *ctx, JSValueConst init_obj)
{
	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_album_class_id);
	JSClassDef album_class = {
		"Album",
		.finalizer = finalizer_album,
	};
	JS_NewClass(rt, nx_album_class_id, &album_class);

	JS_NewClassID(rt, &nx_album_file_class_id);
	JSClassDef album_file_class = {
		"AlbumFile",
		.finalizer = finalizer_album_file,
	};
	JS_NewClass(rt, nx_album_file_class_id, &album_file_class);

	JS_SetPropertyFunctionList(ctx, init_obj, function_list, countof(function_list));
}
