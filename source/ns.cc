#include "error.h"
#include "types.h"
#include "util.h"
#include "wrap.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <switch.h>

using namespace v8;

// libnx weak override: applet exit mode (0 = exit to hbloader for next NRO).
extern "C" u32 __nx_applet_exit_mode;
u32 __nx_applet_exit_mode = 0;

// Defined in main.cc.
extern void nx_exit_event_loop(void);

namespace {

typedef struct {
	char *nro_path;
	void *icon;
	size_t icon_size;
	NacpStruct nacp;
	bool is_nro;
} nx_app_t;

void free_app(nx_app_t *app) {
	if (app->nro_path)
		free(app->nro_path);
	if (app->icon)
		free(app->icon);
	free(app);
}

nx_app_t *get_app(Local<Value> v) { return nx::Unwrap<nx_app_t>(v); }

void nx_ns_exit(const FunctionCallbackInfo<Value> &info) { nsExit(); }

void nx_ns_initialize(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Result rc = nsInitialize();
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "nsInitialize");
		return;
	}
	Local<Context> ctx = iso->GetCurrentContext();
	info.GetReturnValue().Set(
	    FunctionTemplate::New(iso, nx_ns_exit)->GetFunction(ctx)
	        .ToLocalChecked());
}

void nx_ns_app_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_app_t *data = (nx_app_t *)calloc(1, sizeof(nx_app_t));
	u64 application_id = 0;

	if (info[0]->IsNull() || info[0]->IsUndefined()) {
		Result rc = svcGetInfo(&application_id, InfoType_ProgramId,
		                       CUR_PROCESS_HANDLE, 0);
		if (R_FAILED(rc)) {
			free(data);
			nx_throw_libnx_error(iso, rc, "svcGetInfo");
			return;
		}
	} else if (info[0]->IsBigInt()) {
		application_id = info[0].As<BigInt>()->Uint64Value();
	} else if (info[0]->IsString()) {
		String::Utf8Value path(iso, info[0]);
		data->nro_path = strdup(*path ? *path : "");
		FILE *file = fopen(data->nro_path, "rb");
		if (file == NULL) {
			int e = errno;
			free(data->nro_path);
			free(data);
			nx_throw_errno_error(iso, e, "fopen");
			return;
		}
		u32 asset_header_offset;
		fseek(file, 0x18, SEEK_SET);
		fread(&asset_header_offset, sizeof(u32), 1, file);
		uint8_t asset_header[0x28];
		fseek(file, asset_header_offset, SEEK_SET);
		fread(asset_header, 0x28, 1, file);
		u32 icon_section_offset = *(u32 *)(asset_header + 0x8);
		u32 icon_section_size = *(u32 *)(asset_header + 0x10);
		u32 nacp_section_offset = *(u32 *)(asset_header + 0x18);
		u32 nacp_section_size = *(u32 *)(asset_header + 0x20);
		fseek(file, asset_header_offset + icon_section_offset, SEEK_SET);
		data->icon = nx_alloc(iso, icon_section_size);
		if (!data->icon) {
			fclose(file);
			free(data);
			return;
		}
		fread(data->icon, icon_section_size, 1, file);
		fseek(file, asset_header_offset + nacp_section_offset, SEEK_SET);
		fread(&data->nacp, nacp_section_size, 1, file);
		fclose(file);
		data->icon_size = icon_section_size;
		data->is_nro = true;
	} else {
		// ArrayBuffer of NRO contents
		size_t nro_size = 0;
		uint8_t *nro = NX_GetBufferSource(iso, &nro_size, info[0]);
		if (!nro) {
			free(data);
			nx_throw(iso, "expected id, path, or NRO ArrayBuffer");
			return;
		}
		u32 asset_header_offset = *(u32 *)(nro + 0x18);
		uint8_t *asset_header = nro + asset_header_offset;
		u32 icon_section_offset = *(u32 *)(asset_header + 0x8);
		u32 icon_section_size = *(u32 *)(asset_header + 0x10);
		u32 nacp_section_offset = *(u32 *)(asset_header + 0x18);
		u32 nacp_section_size = *(u32 *)(asset_header + 0x20);
		data->icon = nx_alloc(iso, icon_section_size);
		if (!data->icon) {
			free(data);
			return;
		}
		memcpy(data->icon, asset_header + icon_section_offset,
		       icon_section_size);
		memcpy(&data->nacp, asset_header + nacp_section_offset,
		       nacp_section_size);
		data->icon_size = icon_section_size;
		data->is_nro = true;
	}

	if (!data->is_nro) {
		size_t outSize;
		NsApplicationControlData buf;
		Result rc = nsGetApplicationControlData(
		    NsApplicationControlSource_Storage, application_id, &buf,
		    sizeof(NsApplicationControlData), &outSize);
		if (R_FAILED(rc)) {
			free(data);
			nx_throw_libnx_error(iso, rc, "nsGetApplicationControlData");
			return;
		}
		data->icon_size = outSize > 0 ? outSize - sizeof(buf.nacp) : 0;
		data->icon = nx_alloc(iso, data->icon_size);
		if (!data->icon) {
			free(data);
			return;
		}
		memcpy(data->icon, &buf.icon, data->icon_size);
		memcpy(&data->nacp, &buf.nacp, sizeof(NacpStruct));
	}

	Local<Object> app = nx::NewWrapped(iso);
	nx::Wrap<nx_app_t>(iso, app, data, free_app);
	info.GetReturnValue().Set(app);
}

void nx_ns_app_id(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_app_t *app = get_app(info.This());
	if (app)
		info.GetReturnValue().Set(
		    BigInt::NewFromUnsigned(iso, app->nacp.presence_group_id));
}
void nx_ns_app_nacp(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_app_t *app = get_app(info.This());
	if (!app)
		return;
	Local<ArrayBuffer> ab = ArrayBuffer::New(iso, sizeof(app->nacp));
	memcpy(ab->Data(), &app->nacp, sizeof(app->nacp));
	info.GetReturnValue().Set(ab);
}
void nx_ns_app_icon(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_app_t *app = get_app(info.This());
	if (!app || app->icon_size == 0)
		return;
	Local<ArrayBuffer> ab = ArrayBuffer::New(iso, app->icon_size);
	memcpy(ab->Data(), app->icon, app->icon_size);
	info.GetReturnValue().Set(ab);
}
void nx_ns_app_name(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_app_t *app = get_app(info.This());
	if (!app)
		return;
	NacpLanguageEntry *langEntry;
	Result rc = nacpGetLanguageEntry(&app->nacp, &langEntry);
	if (R_FAILED(rc) || !langEntry) {
		nx_throw(iso, "No language entry found");
		return;
	}
	info.GetReturnValue().Set(nx_str(iso, langEntry->name));
}
void nx_ns_app_author(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_app_t *app = get_app(info.This());
	if (!app)
		return;
	NacpLanguageEntry *langEntry;
	Result rc = nacpGetLanguageEntry(&app->nacp, &langEntry);
	if (R_FAILED(rc) || !langEntry) {
		nx_throw(iso, "No language entry found");
		return;
	}
	info.GetReturnValue().Set(nx_str(iso, langEntry->author));
}
void nx_ns_app_version(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_app_t *app = get_app(info.This());
	if (app)
		info.GetReturnValue().Set(nx_str(iso, app->nacp.display_version));
}

void nx_ns_app_next(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	int32_t offset;
	if (!info[0]->Int32Value(iso->GetCurrentContext()).To(&offset))
		return;
	NsApplicationRecord record;
	int record_count = 0;
	Result rc = nsListApplicationRecord(&record, 1, offset, &record_count);
	if (R_FAILED(rc)) {
		nx_throw_libnx_error(iso, rc, "nsListApplicationRecord");
		return;
	}
	if (!record_count) {
		info.GetReturnValue().SetNull();
		return;
	}
	info.GetReturnValue().Set(BigInt::New(iso, record.application_id));
}

void nx_ns_app_launch(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	nx_app_t *app = get_app(info.This());
	if (!app)
		return;
	int argc = info.Length();
	if (app->is_nro) {
		if (!app->nro_path) {
			nx_throw(iso, "NRO path not found");
			return;
		}
		const char *launch_path = app->nro_path;
		char *full_path = NULL;
		if (strncmp(app->nro_path, "sdmc:", 5) != 0) {
			size_t path_len = strlen(app->nro_path) + 6;
			full_path = (char *)nx_alloc(iso, path_len);
			if (!full_path)
				return;
			snprintf(full_path, path_len, "sdmc:%s", app->nro_path);
			launch_path = full_path;
		}
		// Build a quoted argv string: "path" "arg1" "arg2" ...
		std::string args = "\"";
		args += launch_path;
		args += "\"";
		for (int i = 0; i < argc; i++) {
			String::Utf8Value a(iso, info[i]);
			args += " \"";
			for (const char *p = *a ? *a : ""; *p; p++) {
				if (*p == '"')
					args += '\\';
				args += *p;
			}
			args += "\"";
		}
		Result rc = envSetNextLoad(launch_path, args.c_str());
		if (full_path)
			free(full_path);
		if (R_FAILED(rc)) {
			nx_throw_libnx_error(iso, rc, "envSetNextLoad");
			return;
		}
		__nx_applet_exit_mode = 0;
		nx_exit_event_loop();
	} else {
		Result rc =
		    appletRequestLaunchApplication(app->nacp.presence_group_id, NULL);
		if (R_FAILED(rc)) {
			nx_throw_libnx_error(iso, rc, "appletRequestLaunchApplication");
		}
	}
}

void nx_ns_app_init(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> c = iso->GetCurrentContext();
	Local<Object> proto = info[0]
	                          .As<Object>()
	                          ->Get(c, nx_str(iso, "prototype"))
	                          .ToLocalChecked()
	                          .As<Object>();
	NX_DEF_GET(proto, "id", nx_ns_app_id);
	NX_DEF_GET(proto, "nacp", nx_ns_app_nacp);
	NX_DEF_GET(proto, "icon", nx_ns_app_icon);
	NX_DEF_GET(proto, "name", nx_ns_app_name);
	NX_DEF_GET(proto, "author", nx_ns_app_author);
	NX_DEF_GET(proto, "version", nx_ns_app_version);
	NX_DEF_FUNC(proto, "launch", nx_ns_app_launch, 0);
}

} // namespace

void nx_init_ns(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "nsInitialize", nx_ns_initialize);
	NX_SET_FUNC(init_obj, "nsAppNew", nx_ns_app_new);
	NX_SET_FUNC(init_obj, "nsAppInit", nx_ns_app_init);
	NX_SET_FUNC(init_obj, "nsAppNext", nx_ns_app_next);
}
