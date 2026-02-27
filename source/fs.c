#include "fs.h"
#include "async.h"
#include "error.h"
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <sys/stat.h>

static JSClassID nx_file_class_id;

typedef struct {
	FILE *file;
} nx_file_t;

static void finalizer_file(JSRuntime *rt, JSValue val) {
	nx_file_t *file = JS_GetOpaque(val, nx_file_class_id);
	if (file) {
		if (file->file) {
			fclose(file->file);
		}
		js_free_rt(rt, file);
	}
}

typedef struct {
	int err;
	FILE *file;
} nx_fs_fclose_async_t;

typedef struct {
	int err;
	const char *path;
	const char *mode;
	FILE *file;
	long start_offset;
} nx_fs_fopen_async_t;

typedef struct {
	int err;
	FILE *file;
	u8 *buf;
	size_t buf_size;
	JSValue buf_val;
	size_t bytes_read;
	bool eof;
} nx_fs_file_rw_async_t;

typedef struct {
	int err;
	const char *filename;
	u32 start;
	u32 end;
	uint8_t *result;
	size_t size;
} nx_fs_read_file_async_t;

typedef struct {
	int err;
	const char *filename;
	struct stat st;
} nx_fs_stat_async_t;

typedef struct {
	int err;
	const char *filename;
} nx_fs_remove_async_t;

typedef struct {
	int err;
	const char *src;
	const char *dest;
} nx_fs_rename_async_t;

char *dirname(const char *path) {
	if (path == NULL || *path == '\0') {
		return strdup("."); // Current directory for empty paths
	}

	// Copy the path to avoid modifying the original
	char *pathCopy = strdup(path);
	if (pathCopy == NULL) {
		return NULL; // Failed to allocate memory
	}

	// Handle URL-like paths
	char *schemeEnd = strstr(pathCopy, ":/");
	char *start = schemeEnd ? schemeEnd + 2 : pathCopy;

	char *lastSlash = strrchr(start, '/');
	if (lastSlash != NULL) {
		if (lastSlash == start) {
			// Root directory
			*(lastSlash + 1) = '\0';
		} else {
			// Remove everything after the last slash
			*lastSlash = '\0';
		}
	} else {
		if (schemeEnd) {
			// No slash after scheme, return scheme
			*(schemeEnd + 2) = '\0';
		} else {
			// No slashes in path, return "."
			free(pathCopy);
			return strdup(".");
		}
	}

	return pathCopy;
}

int createDirectoryRecursively(char *path, mode_t mode) {
	int created = 0;

	// If a URL path, move the `p` pointer to after the scheme (e.g. after
	// "sdmc:/")
	char *pathStart = strstr(path, ":/");
	char *p = pathStart ? pathStart + 2 : path;

	// Iterate through path segments and create directories as needed
	for (; *p; p++) {
		if (*p == '/') {
			*p = 0; // Temporarily truncate
			if (mkdir(path, mode) == 0) {
				created++;
			} else if (errno != EEXIST) {
				return -1; // Failed to create directory
			}
			*p = '/'; // Restore slash
		}
	}

	// Create the final directory
	if (mkdir(path, mode) == 0) {
		created++;
	} else if (errno != EEXIST) {
		return -1; // Failed to create directory
	}

	return created;
}

void nx_fclose_do(nx_work_t *req) {
	nx_fs_fclose_async_t *data = (nx_fs_fclose_async_t *)req->data;
	data->err = fclose(data->file);
}

JSValue nx_fclose_cb(JSContext *ctx, nx_work_t *req) {
	nx_fs_fclose_async_t *data = (nx_fs_fclose_async_t *)req->data;
	if (data->err) {
		return nx_throw_errno_error(ctx, data->err, "fclose");
	}
	return JS_UNDEFINED;
}

JSValue nx_fclose(JSContext *ctx, JSValueConst this_val, int argc,
				  JSValueConst *argv) {
	nx_file_t *file = JS_GetOpaque2(ctx, argv[0], nx_file_class_id);
	if (!file) {
		return JS_EXCEPTION;
	}
	NX_INIT_WORK_T(nx_fs_fclose_async_t);
	data->file = file->file;
	file->file = NULL;
	return nx_queue_async(ctx, req, nx_fclose_do, nx_fclose_cb);
}

void nx_fopen_do(nx_work_t *req) {
	nx_fs_fopen_async_t *data = (nx_fs_fopen_async_t *)req->data;

	// Create any parent directories (only for write mode)
	if (data->mode[0] == 'w') {
		char *dir = dirname(data->path);
		if (dir) {
			if (createDirectoryRecursively(dir, 0777) == -1) {
				data->err = errno;
				free(dir);
				return;
			}
			free(dir);
		}
	}

	data->file = fopen(data->path, data->mode);
	if (!data->file) {
		data->err = errno;
		return;
	}

	if (data->start_offset > 0) {
		fseek(data->file, data->start_offset, SEEK_SET);
	}
}

JSValue nx_fopen_cb(JSContext *ctx, nx_work_t *req) {
	nx_fs_fopen_async_t *data = (nx_fs_fopen_async_t *)req->data;
	JS_FreeCString(ctx, data->path);
	JS_FreeCString(ctx, data->mode);

	// Throw even on parent directory ENOENT, since the parent
	// dirs were supposed to be created by the worker thread
	if (data->err) {
		return nx_throw_errno_error(ctx, data->err, "fopen");
	}

	JSValue f = JS_NewObjectClass(ctx, nx_file_class_id);
	nx_file_t *file = js_mallocz(ctx, sizeof(nx_file_t));
	file->file = data->file;
	JS_SetOpaque(f, file);
	return f;
}

JSValue nx_fopen(JSContext *ctx, JSValueConst this_val, int argc,
				 JSValueConst *argv) {
	NX_INIT_WORK_T(nx_fs_fopen_async_t);
	data->path = JS_ToCString(ctx, argv[0]);
	data->mode = JS_ToCString(ctx, argv[1]);
	if (!data->path || !data->mode) {
		return JS_EXCEPTION;
	}
	if (argc > 2 && JS_IsNumber(argv[2])) {
		u32 start_offset;
		if (JS_ToUint32(ctx, &start_offset, argv[2])) {
			return JS_EXCEPTION;
		}
		data->start_offset = start_offset;
	}
	return nx_queue_async(ctx, req, nx_fopen_do, nx_fopen_cb);
}

void nx_fread_do(nx_work_t *req) {
	nx_fs_file_rw_async_t *data = (nx_fs_file_rw_async_t *)req->data;
	data->bytes_read = fread(data->buf, 1, data->buf_size, data->file);
	if (ferror(data->file)) {
		data->err = errno;
	} else if (!data->bytes_read && feof(data->file)) {
		data->eof = true;
	}
}

JSValue nx_fread_cb(JSContext *ctx, nx_work_t *req) {
	nx_fs_file_rw_async_t *data = (nx_fs_file_rw_async_t *)req->data;
	JS_FreeValue(ctx, data->buf_val);
	if (data->err) {
		return nx_throw_errno_error(ctx, data->err, "fread");
	}
	return data->eof ? JS_NULL : JS_NewUint32(ctx, data->bytes_read);
}

JSValue nx_fread(JSContext *ctx, JSValueConst this_val, int argc,
				 JSValueConst *argv) {
	nx_file_t *file = JS_GetOpaque2(ctx, argv[0], nx_file_class_id);
	if (!file) {
		return JS_EXCEPTION;
	}
	size_t size;
	u8 *buf = JS_GetArrayBuffer(ctx, &size, argv[1]);
	if (!buf) {
		return JS_EXCEPTION;
	}
	NX_INIT_WORK_T(nx_fs_file_rw_async_t);
	data->file = file->file;
	data->buf = buf;
	data->buf_size = size;
	data->buf_val = JS_DupValue(ctx, argv[1]);
	return nx_queue_async(ctx, req, nx_fread_do, nx_fread_cb);
}

void nx_fwrite_do(nx_work_t *req) {
	nx_fs_file_rw_async_t *data = (nx_fs_file_rw_async_t *)req->data;
	data->bytes_read = fwrite(data->buf, 1, data->buf_size, data->file);
	if (ferror(data->file)) {
		data->err = errno;
	}
}

JSValue nx_fwrite_cb(JSContext *ctx, nx_work_t *req) {
	nx_fs_file_rw_async_t *data = (nx_fs_file_rw_async_t *)req->data;
	JS_FreeValue(ctx, data->buf_val);
	if (data->err) {
		return nx_throw_errno_error(ctx, data->err, "fwrite");
	}
	return JS_NewUint32(ctx, data->bytes_read);
}

JSValue nx_fwrite(JSContext *ctx, JSValueConst this_val, int argc,
				  JSValueConst *argv) {
	nx_file_t *file = JS_GetOpaque2(ctx, argv[0], nx_file_class_id);
	if (!file) {
		return JS_EXCEPTION;
	}
	size_t size;
	u8 *buf = JS_GetArrayBuffer(ctx, &size, argv[1]);
	if (!buf) {
		return JS_EXCEPTION;
	}
	NX_INIT_WORK_T(nx_fs_file_rw_async_t);
	data->file = file->file;
	data->buf = buf;
	data->buf_size = size;
	data->buf_val = JS_DupValue(ctx, argv[1]);
	return nx_queue_async(ctx, req, nx_fwrite_do, nx_fwrite_cb);
}

JSValue nx_mkdir_sync(JSContext *ctx, JSValueConst this_val, int argc,
					  JSValueConst *argv) {
	errno = 0;
	mode_t mode;
	if (JS_ToUint32(ctx, &mode, argv[1])) {
		return JS_EXCEPTION;
	}
	const char *path = JS_ToCString(ctx, argv[0]);
	if (!path) {
		return JS_EXCEPTION;
	}
	int created = createDirectoryRecursively((char *)path, mode);
	JS_FreeCString(ctx, path);
	if (created == -1) {
		return nx_throw_errno_error(ctx, errno, "mkdir");
	}
	return JS_NewInt32(ctx, created);
}

JSValue nx_readdir_sync(JSContext *ctx, JSValueConst this_val, int argc,
						JSValueConst *argv) {
	errno = 0;
	DIR *dir;
	struct dirent *entry;

	const char *path = JS_ToCString(ctx, argv[0]);
	if (!path) {
		return JS_EXCEPTION;
	}
	dir = opendir(path);
	JS_FreeCString(ctx, path);
	if (dir == NULL) {
		if (errno == ENOENT) {
			return JS_NULL;
		}
		return nx_throw_errno_error(ctx, errno, "opendir");
	}

	int i = 0;
	JSValue arr = JS_NewArray(ctx);
	if (JS_IsException(arr)) {
		closedir(dir);
		return arr;
	}
	while ((entry = readdir(dir)) != NULL) {
		// Filter out `.` and `..`
		if (!strcmp(".", entry->d_name) || !strcmp("..", entry->d_name)) {
			continue;
		}
		JSValue str = JS_NewString(ctx, entry->d_name);
		if (JS_IsException(str) || JS_SetPropertyUint32(ctx, arr, i, str) < 0) {
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
		return nx_throw_errno_error(ctx, errno, "readdir");
	}

	closedir(dir);

	return arr;
}

void free_array_buffer(JSRuntime *rt, void *opaque, void *ptr) { free(ptr); }

void free_js_array_buffer(JSRuntime *rt, void *opaque, void *ptr) {
	js_free_rt(rt, ptr);
}

void nx_read_file_do(nx_work_t *req) {
	nx_fs_read_file_async_t *data = (nx_fs_read_file_async_t *)req->data;
	FILE *file = fopen(data->filename, "rb");
	if (file == NULL) {
		data->err = errno;
		return;
	}

	fseek(file, 0, SEEK_END);
	long total_size = ftell(file);

	// Validate and adjust start/end offsets
	if (data->start >= total_size) {
		data->size = 0;
		data->result = NULL;
		fclose(file);
		return;
	}

	if (data->end > total_size) {
		data->end = total_size;
	}

	if (data->start > data->end) {
		data->size = 0;
		data->result = NULL;
		fclose(file);
		return;
	}

	data->size = data->end - data->start;

	// Seek to start offset
	fseek(file, data->start, SEEK_SET);

	data->result = malloc(data->size);
	if (data->result == NULL) {
		data->err = errno;
		fclose(file);
		return;
	}

	size_t result = fread(data->result, 1, data->size, file);
	fclose(file);

	if (result != data->size) {
		free(data->result);
		data->result = NULL;
		data->err = -1;
	}
}

JSValue nx_read_file_cb(JSContext *ctx, nx_work_t *req) {
	nx_fs_read_file_async_t *data = (nx_fs_read_file_async_t *)req->data;
	JS_FreeCString(ctx, data->filename);

	if (data->err == ENOENT) {
		return JS_NULL;
	}

	if (data->err) {
		return nx_throw_errno_error(ctx, data->err, "fread");
	}
	return JS_NewArrayBuffer(ctx, data->result, data->size, free_array_buffer,
							 NULL, false);
}

JSValue nx_read_file(JSContext *ctx, JSValueConst this_val, int argc,
					 JSValueConst *argv) {
	NX_INIT_WORK_T(nx_fs_read_file_async_t);

	data->start = 0;
	data->end = UINT32_MAX;

	if (argc > 1 && JS_IsObject(argv[1])) {
		JSValue start_val = JS_GetPropertyStr(ctx, argv[1], "start");
		JSValue end_val = JS_GetPropertyStr(ctx, argv[1], "end");
		if (JS_IsNumber(start_val) &&
			JS_ToUint32(ctx, &data->start, start_val)) {
			return JS_EXCEPTION;
		}
		if (JS_IsNumber(end_val) && JS_ToUint32(ctx, &data->end, end_val)) {
			return JS_EXCEPTION;
		}
		if (!data->end) {
			data->end = UINT32_MAX;
		}
	}

	data->filename = JS_ToCString(ctx, argv[0]);
	if (!data->filename) {
		return JS_EXCEPTION;
	}

	return nx_queue_async(ctx, req, nx_read_file_do, nx_read_file_cb);
}

JSValue nx_read_file_sync(JSContext *ctx, JSValueConst this_val, int argc,
						  JSValueConst *argv) {
	errno = 0;

	u32 start = 0;
	u32 end = UINT32_MAX;

	if (argc > 1 && JS_IsObject(argv[1])) {
		JSValue start_val = JS_GetPropertyStr(ctx, argv[1], "start");
		JSValue end_val = JS_GetPropertyStr(ctx, argv[1], "end");
		if (JS_IsNumber(start_val) && JS_ToUint32(ctx, &start, start_val)) {
			return JS_EXCEPTION;
		}
		if (JS_IsNumber(end_val) && JS_ToUint32(ctx, &end, end_val)) {
			return JS_EXCEPTION;
		}
	}

	const char *filename = JS_ToCString(ctx, argv[0]);
	if (!filename) {
		return JS_EXCEPTION;
	}

	FILE *file = fopen(filename, "rb");
	if (file == NULL) {
		if (errno == ENOENT) {
			JS_FreeCString(ctx, filename);
			return JS_NULL;
		}
		JS_ThrowTypeError(ctx, "%s: %s", strerror(errno), filename);
		JS_FreeCString(ctx, filename);
		return JS_EXCEPTION;
	}
	JS_FreeCString(ctx, filename);

	fseek(file, 0, SEEK_END);
	size_t total_size = ftell(file);

	// Clamp end to file size if needed
	if (end > total_size) {
		end = total_size;
	}

	// Validate start/end
	if (start > end || start > total_size) {
		fclose(file);
		return JS_ThrowRangeError(ctx, "Invalid range");
	}

	size_t size = end - start;

	// Seek to start position
	fseek(file, start, SEEK_SET);

	uint8_t *buffer = js_malloc(ctx, size);
	if (buffer == NULL) {
		fclose(file);
		return JS_EXCEPTION;
	}

	size_t result = fread(buffer, 1, size, file);
	fclose(file);

	if (result != size) {
		js_free(ctx, buffer);
		JS_ThrowTypeError(
			ctx,
			"Failed to read expected amount of data (got %lu, expected %lu)",
			result, size);
		return JS_EXCEPTION;
	}

	return JS_NewArrayBuffer(ctx, buffer, size, free_js_array_buffer, NULL,
							 false);
}

JSValue nx_write_file_sync(JSContext *ctx, JSValueConst this_val, int argc,
						   JSValueConst *argv) {
	errno = 0;
	const char *filename = JS_ToCString(ctx, argv[0]);

	// Create any parent directories
	char *dir = dirname(filename);
	if (dir) {
		if (createDirectoryRecursively(dir, 0777) == -1) {
			JS_ThrowTypeError(ctx, "%s: %s", strerror(errno), filename);
			free(dir);
			JS_FreeCString(ctx, filename);
			return JS_EXCEPTION;
		}
		free(dir);
	}

	FILE *file = fopen(filename, "w");
	if (file == NULL) {
		JS_ThrowTypeError(ctx, "%s: %s", strerror(errno), filename);
		JS_FreeCString(ctx, filename);
		return JS_EXCEPTION;
	}
	JS_FreeCString(ctx, filename);

	size_t size;
	uint8_t *buffer = JS_GetArrayBuffer(ctx, &size, argv[1]);
	if (buffer == NULL) {
		fclose(file);
		return JS_EXCEPTION;
	}

	size_t result = fwrite(buffer, 1, size, file);
	fclose(file);

	if (result != size) {
		JS_ThrowTypeError(ctx,
						  "Failed to write entire file. Got %lu, expected %lu",
						  result, size);
		return JS_EXCEPTION;
	}

	return JS_UNDEFINED;
}

JSValue statToObject(JSContext *ctx, struct stat *st) {
	JSValue obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, obj, "size", JS_NewInt32(ctx, st->st_size));
	JS_SetPropertyStr(ctx, obj, "mtime", JS_NewInt32(ctx, st->st_mtim.tv_sec));
	JS_SetPropertyStr(ctx, obj, "atime", JS_NewInt32(ctx, st->st_atim.tv_sec));
	JS_SetPropertyStr(ctx, obj, "ctime", JS_NewInt32(ctx, st->st_ctim.tv_sec));
	JS_SetPropertyStr(ctx, obj, "mode", JS_NewInt32(ctx, st->st_mode));
	JS_SetPropertyStr(ctx, obj, "uid", JS_NewInt32(ctx, st->st_uid));
	JS_SetPropertyStr(ctx, obj, "gid", JS_NewInt32(ctx, st->st_gid));
	return obj;
}

void nx_stat_do(nx_work_t *req) {
	nx_fs_stat_async_t *data = (nx_fs_stat_async_t *)req->data;
	if (stat(data->filename, &data->st) != 0) {
		data->err = errno;
	}
}

JSValue nx_stat_cb(JSContext *ctx, nx_work_t *req) {
	nx_fs_stat_async_t *data = (nx_fs_stat_async_t *)req->data;
	JS_FreeCString(ctx, data->filename);

	if (data->err == ENOENT) {
		return JS_NULL;
	}
	if (data->err) {
		return nx_throw_errno_error(ctx, data->err, "stat");
	}
	return statToObject(ctx, &data->st);
}

JSValue nx_stat(JSContext *ctx, JSValueConst this_val, int argc,
				JSValueConst *argv) {
	NX_INIT_WORK_T(nx_fs_stat_async_t);
	data->filename = JS_ToCString(ctx, argv[0]);
	if (!data->filename) {
		return JS_EXCEPTION;
	}
	return nx_queue_async(ctx, req, nx_stat_do, nx_stat_cb);
}

JSValue nx_stat_sync(JSContext *ctx, JSValueConst this_val, int argc,
					 JSValueConst *argv) {
	const char *filename = JS_ToCString(ctx, argv[0]);
	struct stat st;
	int result = stat(filename, &st);
	JS_FreeCString(ctx, filename);
	if (result != 0) {
		if (errno == ENOENT) {
			return JS_NULL;
		}
		return nx_throw_errno_error(ctx, errno, "stat");
	}
	return statToObject(ctx, &st);
}

int removeFileOrDirectory(const char *path) {
	struct stat path_stat;
	int r = stat(path, &path_stat);
	if (r != 0) {
		return errno == ENOENT ? 0 : r;
	}

	if (S_ISREG(path_stat.st_mode)) {
		return unlink(path);
	}

	DIR *d = opendir(path);
	size_t path_len = strlen(path);

	if (d) {
		struct dirent *p;
		r = 0;

		while (!r && (p = readdir(d))) {
			int r2 = -1;
			char *buf;
			size_t len;

			// Skip the names "." and ".." as we don't want to recurse on them.
			if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) {
				continue;
			}

			len = path_len + strlen(p->d_name) + 2;
			buf = malloc(len);

			if (buf) {
				struct stat statbuf;

				snprintf(buf, len, "%s/%s", path, p->d_name);
				if (!stat(buf, &statbuf)) {
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

	if (!r) {
		r = rmdir(path);
	}

	return r;
}

void nx_remove_do(nx_work_t *req) {
	nx_fs_remove_async_t *data = (nx_fs_remove_async_t *)req->data;
	if (removeFileOrDirectory(data->filename) != 0) {
		data->err = errno;
	}
}

JSValue nx_remove_cb(JSContext *ctx, nx_work_t *req) {
	nx_fs_remove_async_t *data = (nx_fs_remove_async_t *)req->data;
	JS_FreeCString(ctx, data->filename);

	if (data->err) {
		return nx_throw_errno_error(ctx, errno, "remove");
	}

	return JS_NULL;
}

JSValue nx_remove(JSContext *ctx, JSValueConst this_val, int argc,
				  JSValueConst *argv) {
	NX_INIT_WORK_T(nx_fs_remove_async_t);
	data->filename = JS_ToCString(ctx, argv[0]);
	if (!data->filename) {
		return JS_EXCEPTION;
	}
	return nx_queue_async(ctx, req, nx_remove_do, nx_remove_cb);
}

JSValue nx_remove_sync(JSContext *ctx, JSValueConst this_val, int argc,
					   JSValueConst *argv) {
	const char *path = JS_ToCString(ctx, argv[0]);
	if (!path)
		return JS_EXCEPTION;
	int result = removeFileOrDirectory(path);
	JS_FreeCString(ctx, path);
	if (result != 0) {
		return nx_throw_errno_error(ctx, errno, "unlink");
	}
	return JS_UNDEFINED;
}

void nx_rename_do(nx_work_t *req) {
	nx_fs_rename_async_t *data = (nx_fs_rename_async_t *)req->data;
	if (rename(data->src, data->dest) != 0) {
		data->err = errno;
	}
}

JSValue nx_rename_cb(JSContext *ctx, nx_work_t *req) {
	nx_fs_rename_async_t *data = (nx_fs_rename_async_t *)req->data;
	JS_FreeCString(ctx, data->src);
	JS_FreeCString(ctx, data->dest);

	if (data->err) {
		return nx_throw_errno_error(ctx, data->err, "rename");
	}

	return JS_NULL;
}

JSValue nx_rename(JSContext *ctx, JSValueConst this_val, int argc,
				  JSValueConst *argv) {
	NX_INIT_WORK_T(nx_fs_rename_async_t);
	data->src = JS_ToCString(ctx, argv[0]);
	if (!data->src) {
		return JS_EXCEPTION;
	}
	data->dest = JS_ToCString(ctx, argv[1]);
	if (!data->dest) {
		return JS_EXCEPTION;
	}
	return nx_queue_async(ctx, req, nx_rename_do, nx_rename_cb);
}

JSValue nx_rename_sync(JSContext *ctx, JSValueConst this_val, int argc,
					   JSValueConst *argv) {
	const char *old_path = JS_ToCString(ctx, argv[0]);
	if (!old_path)
		return JS_EXCEPTION;
	const char *new_path = JS_ToCString(ctx, argv[1]);
	if (!new_path)
		return JS_EXCEPTION;
	int result = rename(old_path, new_path);
	JS_FreeCString(ctx, old_path);
	JS_FreeCString(ctx, new_path);
	if (result != 0) {
		return nx_throw_errno_error(ctx, errno, "rename");
	}
	return JS_UNDEFINED;
}

JSValue nx_fs_create_big_file(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	const char *path = JS_ToCString(ctx, argv[0]);
	if (!path)
		return JS_EXCEPTION;

	char *protocol = js_strdup(ctx, path);
	char *end_of_protocol = strstr(protocol, ":/");
	end_of_protocol[0] = '\0';
	char *name = end_of_protocol + 1;

	FsFileSystem *fs = fsdevGetDeviceFileSystem(protocol);
	if (!fs) {
		js_free(ctx, protocol);
		JS_FreeCString(ctx, path);
		return JS_ThrowTypeError(ctx, "Invalid protocol: %s", protocol);
	}

	Result rc = fsFsCreateFile(fs, name, 0, FsCreateOption_BigFile);
	if (R_FAILED(rc)) {
		js_free(ctx, protocol);
		JS_FreeCString(ctx, path);
		return nx_throw_libnx_error(ctx, rc, "fsFsCreateFile()");
	}

	js_free(ctx, protocol);
	JS_FreeCString(ctx, path);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("fclose", 1, nx_fclose),
	JS_CFUNC_DEF("fopen", 2, nx_fopen),
	JS_CFUNC_DEF("fread", 2, nx_fread),
	JS_CFUNC_DEF("fwrite", 2, nx_fwrite),
	JS_CFUNC_DEF("fsCreateBigFile", 1, nx_fs_create_big_file),
	JS_CFUNC_DEF("mkdirSync", 2, nx_mkdir_sync),
	JS_CFUNC_DEF("readDirSync", 1, nx_readdir_sync),
	JS_CFUNC_DEF("readFile", 1, nx_read_file),
	JS_CFUNC_DEF("readFileSync", 1, nx_read_file_sync),
	JS_CFUNC_DEF("remove", 1, nx_remove),
	JS_CFUNC_DEF("removeSync", 1, nx_remove_sync),
	JS_CFUNC_DEF("rename", 2, nx_rename),
	JS_CFUNC_DEF("renameSync", 2, nx_rename_sync),
	JS_CFUNC_DEF("stat", 1, nx_stat),
	JS_CFUNC_DEF("statSync", 1, nx_stat_sync),
	JS_CFUNC_DEF("writeFileSync", 1, nx_write_file_sync),
};

void nx_init_fs(JSContext *ctx, JSValueConst init_obj) {
	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_file_class_id);
	JSClassDef file_class = {
		"File",
		.finalizer = finalizer_file,
	};
	JS_NewClass(rt, nx_file_class_id, &file_class);

	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
