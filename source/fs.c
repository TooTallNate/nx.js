#include <stdbool.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <errno.h>
#include <sys/stat.h>
#include "fs.h"
#include "async.h"

typedef struct
{
	int err;
	const char *filename;
	uint8_t *result;
	size_t size;
} nx_fs_read_file_async_t;

typedef struct
{
	int err;
	const char *filename;
	struct stat st;
} nx_fs_stat_async_t;

typedef struct
{
	int err;
	const char *filename;
} nx_fs_remove_async_t;

JSValue nx_readdir_sync(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	errno = 0;
	DIR *dir;
	struct dirent *entry;

	const char *path = JS_ToCString(ctx, argv[0]);
	if (!path)
	{
		return JS_Throw(ctx, JS_NewError(ctx));
	}
	dir = opendir(path);
	JS_FreeCString(ctx, path);
	if (dir == NULL)
	{
		JSValue error = JS_NewError(ctx);
		JS_DefinePropertyValueStr(ctx, error, "message", JS_NewString(ctx, strerror(errno)), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
		return JS_Throw(ctx, error);
	}

	int i = 0;
	JSValue arr = JS_NewArray(ctx);
	if (JS_IsException(arr))
	{
		closedir(dir);
		return arr;
	}
	while ((entry = readdir(dir)) != NULL)
	{
		// Filter out `.` and `..`
		if (!strcmp(".", entry->d_name) || !strcmp("..", entry->d_name))
		{
			continue;
		}
		JSValue str = JS_NewString(ctx, entry->d_name);
		if (JS_IsException(str) || JS_SetPropertyUint32(ctx, arr, i, str) < 0)
		{
			JS_FreeValue(ctx, str);
			JS_FreeValue(ctx, arr);
			closedir(dir);
			return JS_EXCEPTION;
		}
		i++;
	}
	if (errno) // Check if readdir ended with an error
	{
		JS_FreeValue(ctx, arr);
		closedir(dir);
		JSValue error = JS_NewError(ctx);
		JS_DefinePropertyValueStr(ctx, error, "message", JS_NewString(ctx, strerror(errno)), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
		return JS_Throw(ctx, error);
	}

	closedir(dir);

	return arr;
}

void free_array_buffer(JSRuntime *rt, void *opaque, void *ptr)
{
	free(ptr);
}

void free_js_array_buffer(JSRuntime *rt, void *opaque, void *ptr)
{
	js_free_rt(rt, ptr);
}

void nx_read_file_do(nx_work_t *req)
{
	nx_fs_read_file_async_t *data = (nx_fs_read_file_async_t *)req->data;
	FILE *file = fopen(data->filename, "rb");
	if (file == NULL)
	{
		data->err = errno;
		return;
	}

	fseek(file, 0, SEEK_END);
	data->size = ftell(file);
	rewind(file);

	data->result = malloc(data->size);
	if (data->result == NULL)
	{
		data->err = errno;
		fclose(file);
		return;
	}

	size_t result = fread(data->result, 1, data->size, file);
	fclose(file);

	if (result != data->size)
	{
		free(data->result);
		data->result = NULL;
		data->err = -1;
	}
}

void nx_read_file_cb(JSContext *ctx, nx_work_t *req, JSValue *args)
{
	nx_fs_read_file_async_t *data = (nx_fs_read_file_async_t *)req->data;
	JS_FreeCString(ctx, data->filename);

	if (data->err)
	{
		args[0] = JS_NewError(ctx);
		JS_DefinePropertyValueStr(ctx, args[0], "message", JS_NewString(ctx, strerror(data->err)), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
		return;
	}

	args[1] = JS_NewArrayBuffer(ctx, data->result, data->size, free_array_buffer, NULL, false);
}

JSValue nx_read_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	NX_INIT_WORK_T(nx_fs_read_file_async_t);
	data->filename = JS_ToCString(ctx, argv[1]);
	nx_queue_async(ctx, req, nx_read_file_do, nx_read_file_cb, argv[0]);
	return JS_UNDEFINED;
}

JSValue nx_read_file_sync(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	errno = 0;
	const char *filename = JS_ToCString(ctx, argv[0]);
	FILE *file = fopen(filename, "rb");
	if (file == NULL)
	{
		JS_ThrowTypeError(ctx, "%s: %s", strerror(errno), filename);
		JS_FreeCString(ctx, filename);
		return JS_EXCEPTION;
	}
	JS_FreeCString(ctx, filename);

	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	rewind(file);

	uint8_t *buffer = js_malloc(ctx, size);
	if (buffer == NULL)
	{
		fclose(file);
		JS_ThrowOutOfMemory(ctx);
		return JS_EXCEPTION;
	}

	size_t result = fread(buffer, 1, size, file);
	fclose(file);

	if (result != size)
	{
		js_free(ctx, buffer);
		JS_ThrowTypeError(ctx, "Failed to read entire file. Got %lu, expected %lu", result, size);
		return JS_EXCEPTION;
	}

	return JS_NewArrayBuffer(ctx, buffer, size, free_js_array_buffer, NULL, false);
}

JSValue nx_write_file_sync(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	errno = 0;
	const char *filename = JS_ToCString(ctx, argv[0]);
	FILE *file = fopen(filename, "w");
	if (file == NULL)
	{
		JS_ThrowTypeError(ctx, "%s: %s", strerror(errno), filename);
		JS_FreeCString(ctx, filename);
		return JS_EXCEPTION;
	}
	JS_FreeCString(ctx, filename);

	size_t size;
	uint8_t *buffer = JS_GetArrayBuffer(ctx, &size, argv[1]);
	if (buffer == NULL)
	{
		fclose(file);
		JS_ThrowOutOfMemory(ctx);
		return JS_EXCEPTION;
	}

	size_t result = fwrite(buffer, 1, size, file);
	fclose(file);

	if (result != size)
	{
		JS_ThrowTypeError(ctx, "Failed to write entire file. Got %lu, expected %lu", result, result);
		return JS_EXCEPTION;
	}

	return JS_UNDEFINED;
}

void nx_stat_do(nx_work_t *req)
{
	nx_fs_stat_async_t *data = (nx_fs_stat_async_t *)req->data;
	if (stat(data->filename, &data->st) != 0)
	{
		data->err = errno;
	}
}

void nx_stat_cb(JSContext *ctx, nx_work_t *req, JSValue *args)
{
	nx_fs_stat_async_t *data = (nx_fs_stat_async_t *)req->data;
	JS_FreeCString(ctx, data->filename);

	if (data->err)
	{
		args[0] = JS_NewError(ctx);
		JS_DefinePropertyValueStr(ctx, args[0], "message", JS_NewString(ctx, strerror(data->err)), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
		return;
	}

	JSValue stat = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, stat, "size", JS_NewInt32(ctx, data->st.st_size));
	JS_SetPropertyStr(ctx, stat, "mtime", JS_NewInt32(ctx, data->st.st_mtim.tv_sec));
	JS_SetPropertyStr(ctx, stat, "atime", JS_NewInt32(ctx, data->st.st_atim.tv_nsec));
	JS_SetPropertyStr(ctx, stat, "ctime", JS_NewInt32(ctx, data->st.st_ctim.tv_nsec));
	JS_SetPropertyStr(ctx, stat, "mode", JS_NewInt32(ctx, data->st.st_mode));
	JS_SetPropertyStr(ctx, stat, "uid", JS_NewInt32(ctx, data->st.st_uid));
	JS_SetPropertyStr(ctx, stat, "gid", JS_NewInt32(ctx, data->st.st_gid));
	args[1] = stat;
}

JSValue nx_stat(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	NX_INIT_WORK_T(nx_fs_stat_async_t);
	data->filename = JS_ToCString(ctx, argv[1]);
	nx_queue_async(ctx, req, nx_stat_do, nx_stat_cb, argv[0]);
	return JS_UNDEFINED;
}

void nx_remove_do(nx_work_t *req)
{
	nx_fs_remove_async_t *data = (nx_fs_remove_async_t *)req->data;
	if (remove(data->filename) != 0)
	{
		data->err = errno;
	}
}

void nx_remove_cb(JSContext *ctx, nx_work_t *req, JSValue *args)
{
	nx_fs_remove_async_t *data = (nx_fs_remove_async_t *)req->data;
	JS_FreeCString(ctx, data->filename);

	if (data->err)
	{
		args[0] = JS_NewError(ctx);
		JS_DefinePropertyValueStr(ctx, args[0], "message", JS_NewString(ctx, strerror(data->err)), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
	}
}

JSValue nx_remove(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	NX_INIT_WORK_T(nx_fs_remove_async_t);
	data->filename = JS_ToCString(ctx, argv[1]);
	nx_queue_async(ctx, req, nx_remove_do, nx_remove_cb, argv[0]);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	// JS_CFUNC_DEF("readDir", 0, nx_readdir),
	JS_CFUNC_DEF("readFile", 2, nx_read_file),
	JS_CFUNC_DEF("readDirSync", 1, nx_readdir_sync),
	JS_CFUNC_DEF("readFileSync", 1, nx_read_file_sync),
	JS_CFUNC_DEF("writeFileSync", 1, nx_write_file_sync),
	JS_CFUNC_DEF("stat", 2, nx_stat),
	JS_CFUNC_DEF("remove", 2, nx_remove)};

void nx_init_fs(JSContext *ctx, JSValueConst init_obj)
{
	JS_SetPropertyFunctionList(ctx, init_obj, function_list, countof(function_list));
}
