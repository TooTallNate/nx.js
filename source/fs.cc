#include "async.h"
#include "error.h"
#include "types.h"
#include "util.h"
#include "wrap.h"
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

using namespace v8;

namespace {

typedef struct {
	FILE *file;
} nx_file_t;

typedef struct {
	DIR *dir;
} nx_dir_t;

// ---- path helpers (unchanged logic from the C version) ----

char *fs_dirname(const char *path) {
	if (path == NULL || *path == '\0') {
		return strdup(".");
	}
	char *pathCopy = strdup(path);
	if (pathCopy == NULL)
		return NULL;
	char *schemeEnd = strstr(pathCopy, ":/");
	char *start = schemeEnd ? schemeEnd + 2 : pathCopy;
	char *lastSlash = strrchr(start, '/');
	if (lastSlash != NULL) {
		if (lastSlash == start) {
			*(lastSlash + 1) = '\0';
		} else {
			*lastSlash = '\0';
		}
	} else {
		if (schemeEnd) {
			*(schemeEnd + 2) = '\0';
		} else {
			free(pathCopy);
			return strdup(".");
		}
	}
	return pathCopy;
}

int createDirectoryRecursively(char *path, mode_t mode) {
	int created = 0;
	char *pathStart = strstr(path, ":/");
	char *p = pathStart ? pathStart + 2 : path;
	for (; *p; p++) {
		if (*p == '/') {
			*p = 0;
			if (mkdir(path, mode) == 0)
				created++;
			else if (errno != EEXIST)
				return -1;
			*p = '/';
		}
	}
	if (mkdir(path, mode) == 0)
		created++;
	else if (errno != EEXIST)
		return -1;
	return created;
}

int removeFileOrDirectory(const char *path) {
	struct stat path_stat;
	int r = stat(path, &path_stat);
	if (r != 0)
		return errno == ENOENT ? 0 : r;
	if (S_ISREG(path_stat.st_mode))
		return unlink(path);
	DIR *d = opendir(path);
	size_t path_len = strlen(path);
	if (d) {
		struct dirent *p;
		r = 0;
		while (!r && (p = readdir(d))) {
			int r2 = -1;
			if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))
				continue;
			size_t len = path_len + strlen(p->d_name) + 2;
			char *buf = (char *)malloc(len);
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
	if (!r)
		r = rmdir(path);
	return r;
}

Local<Object> stat_to_object(Isolate *iso, struct stat *st) {
	Local<Context> context = iso->GetCurrentContext();
	Local<Object> obj = Object::New(iso);
	auto set = [&](const char *k, double v) {
		obj->Set(context, nx_str(iso, k), Number::New(iso, v)).Check();
	};
	set("size", (double)st->st_size);
	set("mtime", (double)st->st_mtim.tv_sec);
	set("atime", (double)st->st_atim.tv_sec);
	set("ctime", (double)st->st_ctim.tv_sec);
	set("mode", (double)st->st_mode);
	set("uid", (double)st->st_uid);
	set("gid", (double)st->st_gid);
	return obj;
}

// ---- File / Dir accessors ----
nx_file_t *get_file(Local<Value> v) { return nx::Unwrap<nx_file_t>(v); }
nx_dir_t *get_dir(Local<Value> v) { return nx::Unwrap<nx_dir_t>(v); }

// ===================== async work payloads =====================
typedef struct {
	int err;
	FILE *file;
} fclose_t;

typedef struct {
	int err;
	char *path;
	char *mode;
	FILE *file;
	long start_offset;
} fopen_t;

typedef struct {
	int err;
	FILE *file;
	uint8_t *buf;
	size_t buf_size;
	Global<Value> buf_val; // keep the ArrayBuffer alive across threads
	size_t bytes;
	bool eof;
} file_rw_t;

typedef struct {
	int err;
	char *filename;
	u32 start;
	u32 end;
	uint8_t *result;
	size_t size;
} read_file_t;

typedef struct {
	int err;
	char *filename;
	uint8_t *buf;
	size_t size;
	Global<Value> buf_val;
} write_file_t;

typedef struct {
	int err;
	char *filename;
	struct stat st;
} stat_t;

typedef struct {
	int err;
	char *path;
	DIR *dir;
} opendir_t;

typedef struct {
	int err;
	DIR *dir;
	char *name;
	unsigned char d_type;
	bool eof;
} readdir_next_t;

typedef struct {
	int err;
	DIR *dir;
} closedir_t;

typedef struct {
	int err;
	char *path;
	mode_t mode;
	int created;
} mkdir_t;

typedef struct {
	int err;
	char *filename;
} remove_t;

typedef struct {
	int err;
	char *src;
	char *dest;
} rename_t;

// ===================== fclose =====================
void fclose_do(nx_work_t *req) {
	fclose_t *d = (fclose_t *)req->data;
	d->err = fclose(d->file);
}
MaybeLocal<Value> fclose_cb(Isolate *iso, nx_work_t *req) {
	fclose_t *d = (fclose_t *)req->data;
	if (d->err) {
		nx_throw_errno_error(iso, d->err, "fclose");
		return MaybeLocal<Value>();
	}
	return Undefined(iso).As<Value>();
}
void nx_fclose(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_file_t *file = get_file(info[0]);
	if (!file)
		return;
	NX_INIT_WORK_T(fclose_t);
	data->file = file->file;
	file->file = NULL;
	info.GetReturnValue().Set(nx_queue_async(iso, req, fclose_do, fclose_cb));
}

// ===================== fopen =====================
void fopen_do(nx_work_t *req) {
	fopen_t *d = (fopen_t *)req->data;
	if (d->mode[0] == 'w') {
		char *dir = fs_dirname(d->path);
		if (dir) {
			if (createDirectoryRecursively(dir, 0777) == -1) {
				d->err = errno;
				free(dir);
				return;
			}
			free(dir);
		}
	}
	d->file = fopen(d->path, d->mode);
	if (!d->file) {
		d->err = errno;
		return;
	}
	if (d->start_offset > 0)
		fseek(d->file, d->start_offset, SEEK_SET);
}
MaybeLocal<Value> fopen_cb(Isolate *iso, nx_work_t *req) {
	fopen_t *d = (fopen_t *)req->data;
	free(d->path);
	free(d->mode);
	if (d->err) {
		nx_throw_errno_error(iso, d->err, "fopen");
		return MaybeLocal<Value>();
	}
	Local<Object> obj = nx::NewWrapped(iso);
	nx_file_t *file = (nx_file_t *)calloc(1, sizeof(nx_file_t));
	file->file = d->file;
	nx::Wrap<nx_file_t>(iso, obj, file, [](nx_file_t *f) {
		if (f->file)
			fclose(f->file);
		free(f);
	});
	return obj.As<Value>();
}
void nx_fopen(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	NX_INIT_WORK_T(fopen_t);
	String::Utf8Value path(iso, info[0]);
	String::Utf8Value mode(iso, info[1]);
	data->path = strdup(*path ? *path : "");
	data->mode = strdup(*mode ? *mode : "r");
	if (info.Length() > 2 && info[2]->IsNumber()) {
		uint32_t off = 0;
		if (info[2]->Uint32Value(context).To(&off))
			data->start_offset = off;
	}
	info.GetReturnValue().Set(nx_queue_async(iso, req, fopen_do, fopen_cb));
}

// ===================== fread / fwrite =====================
void fread_do(nx_work_t *req) {
	file_rw_t *d = (file_rw_t *)req->data;
	d->bytes = fread(d->buf, 1, d->buf_size, d->file);
	if (ferror(d->file))
		d->err = errno;
	else if (!d->bytes && feof(d->file))
		d->eof = true;
}
MaybeLocal<Value> fread_cb(Isolate *iso, nx_work_t *req) {
	file_rw_t *d = (file_rw_t *)req->data;
	d->buf_val.Reset();
	if (d->err) {
		nx_throw_errno_error(iso, d->err, "fread");
		return MaybeLocal<Value>();
	}
	if (d->eof)
		return Null(iso).As<Value>();
	return Integer::NewFromUnsigned(iso, (uint32_t)d->bytes).As<Value>();
}
void fwrite_do(nx_work_t *req) {
	file_rw_t *d = (file_rw_t *)req->data;
	d->bytes = fwrite(d->buf, 1, d->buf_size, d->file);
	if (ferror(d->file))
		d->err = errno;
}
MaybeLocal<Value> fwrite_cb(Isolate *iso, nx_work_t *req) {
	file_rw_t *d = (file_rw_t *)req->data;
	d->buf_val.Reset();
	if (d->err) {
		nx_throw_errno_error(iso, d->err, "fwrite");
		return MaybeLocal<Value>();
	}
	return Integer::NewFromUnsigned(iso, (uint32_t)d->bytes).As<Value>();
}
// Shared setup for fread/fwrite. The work struct has a Global<Value> which is
// non-trivial; NX_INIT_WORK_T uses calloc, so placement-construct it.
void rw_common(const FunctionCallbackInfo<Value> &info, bool write) {
	Isolate *iso = info.GetIsolate();
	nx_file_t *file = get_file(info[0]);
	if (!file)
		return;
	size_t size = 0;
	uint8_t *buf = NX_GetBufferSource(iso, &size, info[1]);
	if (!buf) {
		nx_throw(iso, "expected ArrayBuffer");
		return;
	}
	nx_work_t *req = new nx_work_t();
	file_rw_t *data = new file_rw_t();
	req->data = data;
	data->file = file->file;
	data->buf = buf;
	data->buf_size = size;
	data->buf_val.Reset(iso, info[1]);
	info.GetReturnValue().Set(nx_queue_async(
	    iso, req, write ? fwrite_do : fread_do, write ? fwrite_cb : fread_cb));
}
void nx_fread(const FunctionCallbackInfo<Value> &info) { rw_common(info, false); }
void nx_fwrite(const FunctionCallbackInfo<Value> &info) { rw_common(info, true); }

// ===================== mkdir =====================
void mkdir_do(nx_work_t *req) {
	mkdir_t *d = (mkdir_t *)req->data;
	int created = createDirectoryRecursively(d->path, d->mode);
	if (created == -1)
		d->err = errno;
	else
		d->created = created;
}
MaybeLocal<Value> mkdir_cb(Isolate *iso, nx_work_t *req) {
	mkdir_t *d = (mkdir_t *)req->data;
	free(d->path);
	if (d->err) {
		nx_throw_errno_error(iso, d->err, "mkdir");
		return MaybeLocal<Value>();
	}
	return Integer::New(iso, d->created).As<Value>();
}
void nx_mkdir(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	NX_INIT_WORK_T(mkdir_t);
	uint32_t mode = 0;
	if (info[1]->Uint32Value(context).To(&mode))
		data->mode = mode;
	String::Utf8Value path(iso, info[0]);
	data->path = strdup(*path ? *path : "");
	info.GetReturnValue().Set(nx_queue_async(iso, req, mkdir_do, mkdir_cb));
}
void nx_mkdir_sync(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	errno = 0;
	uint32_t mode = 0;
	if (!info[1]->Uint32Value(context).To(&mode))
		return;
	String::Utf8Value path(iso, info[0]);
	if (!*path)
		return;
	int created = createDirectoryRecursively(*path, mode);
	if (created == -1) {
		nx_throw_errno_error(iso, errno, "mkdir");
		return;
	}
	info.GetReturnValue().Set(Integer::New(iso, created));
}

// ===================== readDirSync =====================
void nx_readdir_sync(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	errno = 0;
	String::Utf8Value path(iso, info[0]);
	if (!*path)
		return;
	DIR *dir = opendir(*path);
	if (dir == NULL) {
		if (errno == ENOENT) {
			info.GetReturnValue().SetNull();
			return;
		}
		nx_throw_errno_error(iso, errno, "opendir");
		return;
	}
	Local<Array> arr = Array::New(iso, 0);
	struct dirent *entry;
	uint32_t i = 0;
	while ((entry = readdir(dir)) != NULL) {
		if (!strcmp(".", entry->d_name) || !strcmp("..", entry->d_name))
			continue;
		arr->Set(context, i++, nx_str(iso, entry->d_name)).Check();
	}
	if (errno) {
		closedir(dir);
		nx_throw_errno_error(iso, errno, "readdir");
		return;
	}
	closedir(dir);
	info.GetReturnValue().Set(arr);
}

// ===================== readFile (async) =====================
void read_file_do(nx_work_t *req) {
	read_file_t *d = (read_file_t *)req->data;
	FILE *file = fopen(d->filename, "rb");
	if (file == NULL) {
		d->err = errno;
		return;
	}
	fseek(file, 0, SEEK_END);
	long total_size = ftell(file);
	if ((long)d->start >= total_size) {
		d->size = 0;
		d->result = NULL;
		fclose(file);
		return;
	}
	if (d->end > total_size)
		d->end = total_size;
	if (d->start > d->end) {
		d->size = 0;
		d->result = NULL;
		fclose(file);
		return;
	}
	d->size = d->end - d->start;
	fseek(file, d->start, SEEK_SET);
	d->result = (uint8_t *)malloc(d->size);
	if (d->result == NULL) {
		d->err = errno;
		fclose(file);
		return;
	}
	size_t result = fread(d->result, 1, d->size, file);
	fclose(file);
	if (result != d->size) {
		free(d->result);
		d->result = NULL;
		d->err = -1;
	}
}
MaybeLocal<Value> read_file_cb(Isolate *iso, nx_work_t *req) {
	read_file_t *d = (read_file_t *)req->data;
	free(d->filename);
	if (d->err == ENOENT)
		return Null(iso).As<Value>();
	if (d->err) {
		nx_throw_errno_error(iso, d->err, "fread");
		return MaybeLocal<Value>();
	}
	uint8_t *buf = d->result;
	size_t size = d->size;
	d->result = nullptr;
	std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
	    buf, size, [](void *p, size_t, void *) { free(p); }, nullptr);
	return ArrayBuffer::New(iso, std::move(bs)).As<Value>();
}
// Parse optional {start,end} options object into start/end.
bool parse_range(Isolate *iso, Local<Value> opts, u32 *start, u32 *end) {
	Local<Context> context = iso->GetCurrentContext();
	if (!opts->IsObject())
		return true;
	Local<Object> o = opts.As<Object>();
	Local<Value> sv, ev;
	if (o->Get(context, nx_str(iso, "start")).ToLocal(&sv) && sv->IsNumber()) {
		if (!sv->Uint32Value(context).To(start))
			return false;
	}
	if (o->Get(context, nx_str(iso, "end")).ToLocal(&ev) && ev->IsNumber()) {
		if (!ev->Uint32Value(context).To(end))
			return false;
	}
	return true;
}
void nx_read_file(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	NX_INIT_WORK_T(read_file_t);
	data->start = 0;
	data->end = UINT32_MAX;
	if (info.Length() > 1 && !parse_range(iso, info[1], &data->start, &data->end)) {
		free(data);
		delete req;
		return;
	}
	if (!data->end)
		data->end = UINT32_MAX;
	String::Utf8Value filename(iso, info[0]);
	data->filename = strdup(*filename ? *filename : "");
	info.GetReturnValue().Set(
	    nx_queue_async(iso, req, read_file_do, read_file_cb));
}
void nx_read_file_sync(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	errno = 0;
	u32 start = 0, end = UINT32_MAX;
	if (info.Length() > 1 && !parse_range(iso, info[1], &start, &end))
		return;
	String::Utf8Value filename(iso, info[0]);
	if (!*filename)
		return;
	FILE *file = fopen(*filename, "rb");
	if (file == NULL) {
		if (errno == ENOENT) {
			info.GetReturnValue().SetNull();
			return;
		}
		nx_throw_errno_error(iso, errno, "fopen");
		return;
	}
	fseek(file, 0, SEEK_END);
	size_t total_size = ftell(file);
	if (end > total_size)
		end = total_size;
	if (start > end || start > total_size) {
		fclose(file);
		iso->ThrowException(Exception::RangeError(nx_str(iso, "Invalid range")));
		return;
	}
	size_t size = end - start;
	fseek(file, start, SEEK_SET);
	uint8_t *buffer = (uint8_t *)malloc(size);
	if (buffer == NULL) {
		fclose(file);
		nx_throw(iso, "out of memory");
		return;
	}
	size_t result = fread(buffer, 1, size, file);
	fclose(file);
	if (result != size) {
		free(buffer);
		nx_throw(iso, "Failed to read expected amount of data");
		return;
	}
	std::unique_ptr<BackingStore> bs = ArrayBuffer::NewBackingStore(
	    buffer, size, [](void *p, size_t, void *) { free(p); }, nullptr);
	info.GetReturnValue().Set(ArrayBuffer::New(iso, std::move(bs)));
}

// ===================== writeFile =====================
void write_file_do(nx_work_t *req) {
	write_file_t *d = (write_file_t *)req->data;
	char *dir = fs_dirname(d->filename);
	if (dir) {
		if (createDirectoryRecursively(dir, 0777) == -1) {
			d->err = errno;
			free(dir);
			return;
		}
		free(dir);
	}
	FILE *file = fopen(d->filename, "w");
	if (file == NULL) {
		d->err = errno;
		return;
	}
	size_t result = fwrite(d->buf, 1, d->size, file);
	fclose(file);
	if (result != d->size)
		d->err = -1;
}
MaybeLocal<Value> write_file_cb(Isolate *iso, nx_work_t *req) {
	write_file_t *d = (write_file_t *)req->data;
	free(d->filename);
	d->buf_val.Reset();
	if (d->err) {
		nx_throw_errno_error(iso, d->err, "fwrite");
		return MaybeLocal<Value>();
	}
	return Undefined(iso).As<Value>();
}
void nx_write_file(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	size_t size = 0;
	uint8_t *buf = NX_GetBufferSource(iso, &size, info[1]);
	if (!buf) {
		nx_throw(iso, "expected ArrayBuffer");
		return;
	}
	nx_work_t *req = new nx_work_t();
	write_file_t *data = new write_file_t();
	req->data = data;
	String::Utf8Value filename(iso, info[0]);
	data->filename = strdup(*filename ? *filename : "");
	data->buf = buf;
	data->size = size;
	data->buf_val.Reset(iso, info[1]);
	info.GetReturnValue().Set(
	    nx_queue_async(iso, req, write_file_do, write_file_cb));
}
// Shared impl for writeFileSync (truncate) and appendFileSync. The optional
// 3rd argument selects append mode ("a" vs "w").
static void nx_write_file_sync_impl(const FunctionCallbackInfo<Value> &info,
                                    bool append) {
	Isolate *iso = info.GetIsolate();
	errno = 0;
	String::Utf8Value filename(iso, info[0]);
	if (!*filename)
		return;
	char *dir = fs_dirname(*filename);
	if (dir) {
		if (createDirectoryRecursively(dir, 0777) == -1) {
			free(dir);
			nx_throw_errno_error(iso, errno, "mkdir");
			return;
		}
		free(dir);
	}
	FILE *file = fopen(*filename, append ? "a" : "w");
	if (file == NULL) {
		nx_throw_errno_error(iso, errno, "fopen");
		return;
	}
	size_t size = 0;
	uint8_t *buffer = NX_GetBufferSource(iso, &size, info[1]);
	if (buffer == NULL) {
		fclose(file);
		nx_throw(iso, "expected ArrayBuffer");
		return;
	}
	size_t result = fwrite(buffer, 1, size, file);
	fclose(file);
	if (result != size) {
		nx_throw(iso, "Failed to write entire file");
	}
}

void nx_write_file_sync(const FunctionCallbackInfo<Value> &info) {
	nx_write_file_sync_impl(info, false);
}

void nx_append_file_sync(const FunctionCallbackInfo<Value> &info) {
	nx_write_file_sync_impl(info, true);
}

// ===================== openDir / readDirNext / closeDir =====================
void opendir_do(nx_work_t *req) {
	opendir_t *d = (opendir_t *)req->data;
	d->dir = opendir(d->path);
	if (!d->dir)
		d->err = errno;
}
MaybeLocal<Value> opendir_cb(Isolate *iso, nx_work_t *req) {
	opendir_t *d = (opendir_t *)req->data;
	free(d->path);
	if (d->err) {
		nx_throw_errno_error(iso, d->err, "opendir");
		return MaybeLocal<Value>();
	}
	Local<Object> obj = nx::NewWrapped(iso);
	nx_dir_t *dir = (nx_dir_t *)calloc(1, sizeof(nx_dir_t));
	dir->dir = d->dir;
	nx::Wrap<nx_dir_t>(iso, obj, dir, [](nx_dir_t *x) {
		if (x->dir)
			closedir(x->dir);
		free(x);
	});
	return obj.As<Value>();
}
void nx_opendir(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	NX_INIT_WORK_T(opendir_t);
	String::Utf8Value path(iso, info[0]);
	data->path = strdup(*path ? *path : "");
	info.GetReturnValue().Set(nx_queue_async(iso, req, opendir_do, opendir_cb));
}

void readdir_next_do(nx_work_t *req) {
	readdir_next_t *d = (readdir_next_t *)req->data;
	errno = 0;
	struct dirent *entry = readdir(d->dir);
	while (entry &&
	       (!strcmp(".", entry->d_name) || !strcmp("..", entry->d_name))) {
		entry = readdir(d->dir);
	}
	if (entry) {
		d->name = strdup(entry->d_name);
		if (!d->name)
			d->err = ENOMEM;
		else
			d->d_type = entry->d_type;
	} else if (errno) {
		d->err = errno;
	} else {
		d->eof = true;
	}
}
MaybeLocal<Value> readdir_next_cb(Isolate *iso, nx_work_t *req) {
	Local<Context> context = iso->GetCurrentContext();
	readdir_next_t *d = (readdir_next_t *)req->data;
	if (d->err) {
		nx_throw_errno_error(iso, d->err, "readdir");
		return MaybeLocal<Value>();
	}
	if (d->eof)
		return Null(iso).As<Value>();
	Local<Object> obj = Object::New(iso);
	obj->Set(context, nx_str(iso, "name"), nx_str(iso, d->name)).Check();
	obj->Set(context, nx_str(iso, "isFile"),
	         Boolean::New(iso, d->d_type == DT_REG))
	    .Check();
	obj->Set(context, nx_str(iso, "isDirectory"),
	         Boolean::New(iso, d->d_type == DT_DIR))
	    .Check();
	obj->Set(context, nx_str(iso, "isSymlink"),
	         Boolean::New(iso, d->d_type == DT_LNK))
	    .Check();
	free(d->name);
	d->name = nullptr;
	return obj.As<Value>();
}
void nx_readdir_next(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_dir_t *dir = get_dir(info[0]);
	if (!dir)
		return;
	NX_INIT_WORK_T(readdir_next_t);
	data->dir = dir->dir;
	info.GetReturnValue().Set(
	    nx_queue_async(iso, req, readdir_next_do, readdir_next_cb));
}

void closedir_do(nx_work_t *req) {
	closedir_t *d = (closedir_t *)req->data;
	if (closedir(d->dir) != 0)
		d->err = errno;
}
MaybeLocal<Value> closedir_cb(Isolate *iso, nx_work_t *req) {
	closedir_t *d = (closedir_t *)req->data;
	if (d->err) {
		nx_throw_errno_error(iso, d->err, "closedir");
		return MaybeLocal<Value>();
	}
	return Undefined(iso).As<Value>();
}
void nx_closedir(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_dir_t *dir = get_dir(info[0]);
	if (!dir)
		return;
	NX_INIT_WORK_T(closedir_t);
	data->dir = dir->dir;
	dir->dir = NULL; // prevent finalizer double-close
	info.GetReturnValue().Set(
	    nx_queue_async(iso, req, closedir_do, closedir_cb));
}

// ===================== stat =====================
void stat_do(nx_work_t *req) {
	stat_t *d = (stat_t *)req->data;
	if (stat(d->filename, &d->st) != 0)
		d->err = errno;
}
MaybeLocal<Value> stat_cb(Isolate *iso, nx_work_t *req) {
	stat_t *d = (stat_t *)req->data;
	free(d->filename);
	if (d->err == ENOENT)
		return Null(iso).As<Value>();
	if (d->err) {
		nx_throw_errno_error(iso, d->err, "stat");
		return MaybeLocal<Value>();
	}
	return stat_to_object(iso, &d->st).As<Value>();
}
void nx_stat(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	NX_INIT_WORK_T(stat_t);
	String::Utf8Value filename(iso, info[0]);
	data->filename = strdup(*filename ? *filename : "");
	info.GetReturnValue().Set(nx_queue_async(iso, req, stat_do, stat_cb));
}
void nx_stat_sync(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value filename(iso, info[0]);
	struct stat st;
	int result = stat(*filename ? *filename : "", &st);
	if (result != 0) {
		if (errno == ENOENT) {
			info.GetReturnValue().SetNull();
			return;
		}
		nx_throw_errno_error(iso, errno, "stat");
		return;
	}
	info.GetReturnValue().Set(stat_to_object(iso, &st));
}

// ===================== remove / rename =====================
void remove_do(nx_work_t *req) {
	remove_t *d = (remove_t *)req->data;
	if (removeFileOrDirectory(d->filename) != 0)
		d->err = errno;
}
MaybeLocal<Value> remove_cb(Isolate *iso, nx_work_t *req) {
	remove_t *d = (remove_t *)req->data;
	free(d->filename);
	if (d->err) {
		nx_throw_errno_error(iso, d->err, "remove");
		return MaybeLocal<Value>();
	}
	return Null(iso).As<Value>();
}
void nx_remove(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	NX_INIT_WORK_T(remove_t);
	String::Utf8Value filename(iso, info[0]);
	data->filename = strdup(*filename ? *filename : "");
	info.GetReturnValue().Set(nx_queue_async(iso, req, remove_do, remove_cb));
}
void nx_remove_sync(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value path(iso, info[0]);
	if (!*path)
		return;
	if (removeFileOrDirectory(*path) != 0) {
		nx_throw_errno_error(iso, errno, "unlink");
	}
}

void rename_do(nx_work_t *req) {
	rename_t *d = (rename_t *)req->data;
	if (rename(d->src, d->dest) != 0)
		d->err = errno;
}
MaybeLocal<Value> rename_cb(Isolate *iso, nx_work_t *req) {
	rename_t *d = (rename_t *)req->data;
	free(d->src);
	free(d->dest);
	if (d->err) {
		nx_throw_errno_error(iso, d->err, "rename");
		return MaybeLocal<Value>();
	}
	return Null(iso).As<Value>();
}
void nx_rename(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	NX_INIT_WORK_T(rename_t);
	String::Utf8Value src(iso, info[0]);
	String::Utf8Value dest(iso, info[1]);
	data->src = strdup(*src ? *src : "");
	data->dest = strdup(*dest ? *dest : "");
	info.GetReturnValue().Set(nx_queue_async(iso, req, rename_do, rename_cb));
}
void nx_rename_sync(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value old_path(iso, info[0]);
	String::Utf8Value new_path(iso, info[1]);
	if (rename(*old_path ? *old_path : "", *new_path ? *new_path : "") != 0) {
		nx_throw_errno_error(iso, errno, "rename");
	}
}

// ===================== fsCreateBigFile =====================
void nx_fs_create_big_file(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	String::Utf8Value path(iso, info[0]);
	if (!*path)
		return;
	char *protocol = strdup(*path);
	char *end_of_protocol = strstr(protocol, ":/");
	if (!end_of_protocol) {
		free(protocol);
		nx_throw(iso, "Invalid protocol");
		return;
	}
	end_of_protocol[0] = '\0';
	char *name = end_of_protocol + 1;
	FsFileSystem *fs = fsdevGetDeviceFileSystem(protocol);
	if (!fs) {
		free(protocol);
		nx_throw(iso, "Invalid protocol");
		return;
	}
	Result rc = fsFsCreateFile(fs, name, 0, FsCreateOption_BigFile);
	free(protocol);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "fsFsCreateFile");
	}
}

} // namespace

void nx_init_fs(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "fclose", nx_fclose);
	NX_SET_FUNC(init_obj, "fopen", nx_fopen);
	NX_SET_FUNC(init_obj, "fread", nx_fread);
	NX_SET_FUNC(init_obj, "fwrite", nx_fwrite);
	NX_SET_FUNC(init_obj, "closeDir", nx_closedir);
	NX_SET_FUNC(init_obj, "fsCreateBigFile", nx_fs_create_big_file);
	NX_SET_FUNC(init_obj, "mkdir", nx_mkdir);
	NX_SET_FUNC(init_obj, "openDir", nx_opendir);
	NX_SET_FUNC(init_obj, "mkdirSync", nx_mkdir_sync);
	NX_SET_FUNC(init_obj, "readDirSync", nx_readdir_sync);
	NX_SET_FUNC(init_obj, "readDirNext", nx_readdir_next);
	NX_SET_FUNC(init_obj, "readFile", nx_read_file);
	NX_SET_FUNC(init_obj, "readFileSync", nx_read_file_sync);
	NX_SET_FUNC(init_obj, "remove", nx_remove);
	NX_SET_FUNC(init_obj, "removeSync", nx_remove_sync);
	NX_SET_FUNC(init_obj, "rename", nx_rename);
	NX_SET_FUNC(init_obj, "renameSync", nx_rename_sync);
	NX_SET_FUNC(init_obj, "stat", nx_stat);
	NX_SET_FUNC(init_obj, "statSync", nx_stat_sync);
	NX_SET_FUNC(init_obj, "writeFile", nx_write_file);
	NX_SET_FUNC(init_obj, "writeFileSync", nx_write_file_sync);
	NX_SET_FUNC(init_obj, "appendFileSync", nx_append_file_sync);
}
