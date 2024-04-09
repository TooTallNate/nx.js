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

char *dirname(const char *path)
{
	if (path == NULL || *path == '\0')
	{
		return strdup("."); // Current directory for empty paths
	}

	// Copy the path to avoid modifying the original
	char *pathCopy = strdup(path);
	if (pathCopy == NULL)
	{
		return NULL; // Failed to allocate memory
	}

	// Handle URL-like paths
	char *schemeEnd = strstr(pathCopy, ":/");
	char *start = schemeEnd ? schemeEnd + 2 : pathCopy;

	char *lastSlash = strrchr(start, '/');
	if (lastSlash != NULL)
	{
		if (lastSlash == start)
		{
			// Root directory
			*(lastSlash + 1) = '\0';
		}
		else
		{
			// Remove everything after the last slash
			*lastSlash = '\0';
		}
	}
	else
	{
		if (schemeEnd)
		{
			// No slash after scheme, return scheme
			*(schemeEnd + 2) = '\0';
		}
		else
		{
			// No slashes in path, return "."
			free(pathCopy);
			return strdup(".");
		}
	}

	return pathCopy;
}

int createDirectoryRecursively(char *path, mode_t mode)
{
	int created = 0;

	// If a URL path, move the `p` pointer to after the scheme (e.g. after "sdmc:/")
	char *pathStart = strstr(path, ":/");
	char *p = pathStart ? pathStart + 2 : path;

	// Iterate through path segments and create directories as needed
	for (; *p; p++)
	{
		if (*p == '/')
		{
			*p = 0; // Temporarily truncate
			if (mkdir(path, mode) == 0)
			{
				created++;
			}
			else if (errno != EEXIST)
			{
				return -1; // Failed to create directory
			}
			*p = '/'; // Restore slash
		}
	}

	// Create the final directory
	if (mkdir(path, mode) == 0)
	{
		created++;
	}
	else if (errno != EEXIST)
	{
		return -1; // Failed to create directory
	}

	return created;
}

JSValue nx_mkdir_sync(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	errno = 0;
	mode_t mode;
	if (JS_ToUint32(ctx, &mode, argv[1]))
	{
		return JS_EXCEPTION;
	}
	const char *path = JS_ToCString(ctx, argv[0]);
	if (!path)
	{
		return JS_EXCEPTION;
	}
	int created = createDirectoryRecursively((char *)path, mode);
	JS_FreeCString(ctx, path);
	if (created == -1)
	{
		JSValue error = JS_NewError(ctx);
		JS_DefinePropertyValueStr(ctx, error, "message", JS_NewString(ctx, strerror(errno)), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
		return JS_Throw(ctx, error);
	}
	return JS_NewInt32(ctx, created);
}

JSValue nx_readdir_sync(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	errno = 0;
	DIR *dir;
	struct dirent *entry;

	const char *path = JS_ToCString(ctx, argv[0]);
	if (!path)
	{
		return JS_EXCEPTION;
	}
	dir = opendir(path);
	JS_FreeCString(ctx, path);
	if (dir == NULL)
	{
		if (errno == ENOENT)
		{
			return JS_NULL;
		}
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

	if (data->err == ENOENT)
	{
		args[1] = JS_NULL;
	}
	else if (data->err)
	{
		JS_DefinePropertyValueStr(ctx, args[0], "message", JS_NewString(ctx, strerror(data->err)), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
	}
	else
	{
		args[1] = JS_NewArrayBuffer(ctx, data->result, data->size, free_array_buffer, NULL, false);
	}
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
		if (errno == ENOENT)
		{
			JS_FreeCString(ctx, filename);
			return JS_NULL;
		}
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

	// Create any parent directories
	char *dir = dirname(filename);
	if (dir)
	{
		if (createDirectoryRecursively(dir, 0777) == -1)
		{
			JS_ThrowTypeError(ctx, "%s: %s", strerror(errno), filename);
			free(dir);
			JS_FreeCString(ctx, filename);
			return JS_EXCEPTION;
		}
		free(dir);
	}

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
		return JS_EXCEPTION;
	}

	size_t result = fwrite(buffer, 1, size, file);
	fclose(file);

	if (result != size)
	{
		JS_ThrowTypeError(ctx, "Failed to write entire file. Got %lu, expected %lu", result, size);
		return JS_EXCEPTION;
	}

	return JS_UNDEFINED;
}

JSValue statToObject(JSContext *ctx, struct stat *st)
{
	JSValue obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, obj, "size", JS_NewInt32(ctx, st->st_size));
	JS_SetPropertyStr(ctx, obj, "mtime", JS_NewInt32(ctx, st->st_mtim.tv_sec));
	JS_SetPropertyStr(ctx, obj, "atime", JS_NewInt32(ctx, st->st_atim.tv_nsec));
	JS_SetPropertyStr(ctx, obj, "ctime", JS_NewInt32(ctx, st->st_ctim.tv_nsec));
	JS_SetPropertyStr(ctx, obj, "mode", JS_NewInt32(ctx, st->st_mode));
	JS_SetPropertyStr(ctx, obj, "uid", JS_NewInt32(ctx, st->st_uid));
	JS_SetPropertyStr(ctx, obj, "gid", JS_NewInt32(ctx, st->st_gid));
	return obj;
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

	if (data->err == ENOENT) {
		args[1] = JS_NULL;
	}
	else if (data->err)
	{
		args[0] = JS_NewError(ctx);
		JS_DefinePropertyValueStr(ctx, args[0], "message", JS_NewString(ctx, strerror(data->err)), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
		return;
	} else {
		args[1] = statToObject(ctx, &data->st);
	}
}

JSValue nx_stat(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	NX_INIT_WORK_T(nx_fs_stat_async_t);
	data->filename = JS_ToCString(ctx, argv[1]);
	nx_queue_async(ctx, req, nx_stat_do, nx_stat_cb, argv[0]);
	return JS_UNDEFINED;
}

JSValue nx_stat_sync(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *filename = JS_ToCString(ctx, argv[0]);
	struct stat st;
	int result = stat(filename, &st);
	JS_FreeCString(ctx, filename);
	if (result != 0)
	{
		if (errno == ENOENT)
		{
			return JS_NULL;
		}
		JSValue error = JS_NewError(ctx);
		JS_DefinePropertyValueStr(ctx, error, "message", JS_NewString(ctx, strerror(errno)), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
		return JS_Throw(ctx, error);
	}
	return statToObject(ctx, &st);
}

int removeFileOrDirectory(const char *path)
{
	struct stat path_stat;
	int r = stat(path, &path_stat);
	if (r != 0)
	{
		return errno == ENOENT ? 0 : r;
	}

	if (S_ISREG(path_stat.st_mode))
	{
		return unlink(path);
	}

	DIR *d = opendir(path);
	size_t path_len = strlen(path);

	if (d)
	{
		struct dirent *p;
		r = 0;

		while (!r && (p = readdir(d)))
		{
			int r2 = -1;
			char *buf;
			size_t len;

			// Skip the names "." and ".." as we don't want to recurse on them.
			if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
			{
				continue;
			}

			len = path_len + strlen(p->d_name) + 2;
			buf = malloc(len);

			if (buf)
			{
				struct stat statbuf;

				snprintf(buf, len, "%s/%s", path, p->d_name);
				if (!stat(buf, &statbuf))
				{
					if (S_ISDIR(statbuf.st_mode))
						r2 = removeFileOrDirectory(buf);
					else
						r2 = unlink(buf);
				}

				free(buf);
			}

			r = r2;
		}

		closedir(d);
	}

	if (!r)
	{
		r = rmdir(path);
	}

	return r;
}

void nx_remove_do(nx_work_t *req)
{
	nx_fs_remove_async_t *data = (nx_fs_remove_async_t *)req->data;
	if (removeFileOrDirectory(data->filename) != 0)
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
	if (!data->filename)
	{
		return JS_EXCEPTION;
	}
	nx_queue_async(ctx, req, nx_remove_do, nx_remove_cb, argv[0]);
	return JS_UNDEFINED;
}

JSValue nx_remove_sync(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *path = JS_ToCString(ctx, argv[0]);
	if (!path)
		return JS_EXCEPTION;
	int result = removeFileOrDirectory(path);
	JS_FreeCString(ctx, path);
	if (result != 0)
	{
		JSValue error = JS_NewError(ctx);
		JS_DefinePropertyValueStr(ctx, error, "message", JS_NewString(ctx, strerror(errno)), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
		return JS_Throw(ctx, error);
	}
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("mkdirSync", 1, nx_mkdir_sync),
	JS_CFUNC_DEF("readDirSync", 1, nx_readdir_sync),
	JS_CFUNC_DEF("readFile", 2, nx_read_file),
	JS_CFUNC_DEF("readFileSync", 1, nx_read_file_sync),
	JS_CFUNC_DEF("remove", 2, nx_remove),
	JS_CFUNC_DEF("removeSync", 2, nx_remove_sync),
	JS_CFUNC_DEF("stat", 2, nx_stat),
	JS_CFUNC_DEF("statSync", 2, nx_stat_sync),
	JS_CFUNC_DEF("writeFileSync", 1, nx_write_file_sync),
};

void nx_init_fs(JSContext *ctx, JSValueConst init_obj)
{
	JS_SetPropertyFunctionList(ctx, init_obj, function_list, countof(function_list));
}
