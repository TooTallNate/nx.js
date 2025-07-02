#include "ns.h"
#include "applet.h"
#include "error.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <switch.h>

u32 __nx_applet_exit_mode = 0;

// External function to cleanly exit the main event loop
extern void nx_exit_event_loop(void);

static JSClassID nx_app_class_id;

nx_app_t *nx_get_app(JSContext *ctx, JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_app_class_id);
}

static void finalizer_app(JSRuntime *rt, JSValue val) {
	nx_app_t *app = JS_GetOpaque(val, nx_app_class_id);
	if (app) {
		if (app->nro_path) {
			js_free_rt(rt, app->nro_path);
		}
		if (app->icon) {
			js_free_rt(rt, app->icon);
		}
		js_free_rt(rt, app);
	}
}

static JSValue nx_ns_exit(JSContext *ctx, JSValueConst this_val, int argc,
						  JSValueConst *argv) {
	nsExit();
	return JS_UNDEFINED;
}

static JSValue nx_ns_initialize(JSContext *ctx, JSValueConst this_val, int argc,
								JSValueConst *argv) {
	Result rc = nsInitialize();
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "nsInitialize()");
	}
	return JS_NewCFunction(ctx, nx_ns_exit, "nsExit", 0);
}

static JSValue nx_ns_app_new(JSContext *ctx, JSValueConst this_val, int argc,
							 JSValueConst *argv) {
	nx_app_t *data = js_mallocz(ctx, sizeof(nx_app_t));
	if (!data) {
		return JS_EXCEPTION;
	}
	u64 application_id = 0;
	if (JS_IsNull(argv[0])) {
		// Get own app ID from the running process.
		// This is the case for NSP installations, or when running via an
		// emulator (i.e. Ryujinx).
		Result rc = svcGetInfo(&application_id, InfoType_ProgramId,
							   CUR_PROCESS_HANDLE, 0);
		if (R_FAILED(rc)) {
			js_free(ctx, data);
			return nx_throw_libnx_error(ctx, rc, "svcGetInfo()");
		}
	} else if (JS_IsBigInt(ctx, argv[0])) {
		if (JS_ToBigUint64(ctx, &application_id, argv[0])) {
			js_free(ctx, data);
			return JS_EXCEPTION;
		}
	} else if (JS_IsString(argv[0])) {
		// Get app ID from opening the path specified at the string value, which
		// contains an NRO file.
		const char *path = JS_ToCString(ctx, argv[0]);
		if (!path) {
			js_free(ctx, data);
			return JS_EXCEPTION;
		}
		data->nro_path = js_strdup(ctx, path);
		if (!data->nro_path) {
			js_free(ctx, data);
			return JS_EXCEPTION;
		}
		FILE *file = fopen(path, "rb");
		JS_FreeCString(ctx, path);

		if (file == NULL) {
			js_free(ctx, data->nro_path);
			js_free(ctx, data);
			return nx_throw_errno_error(ctx, errno, "fopen()");
		}

		// Seek to offset 0x18 and read a u32 from the NRO file, which
		// contains the offset of the asset header.
		u32 asset_header_offset;
		fseek(file, 0x18, SEEK_SET);
		fread(&asset_header_offset, sizeof(u32), 1, file);

		// Seek the file to the asset header offset and allocate buffer
		uint8_t *asset_header = js_malloc(ctx, 0x28); // Size needed for header info
		fseek(file, asset_header_offset, SEEK_SET);
		fread(asset_header, 0x28, 1, file);

		u32 icon_section_offset = *(u32 *)(asset_header + 0x8);
		u32 icon_section_size = *(u32 *)(asset_header + 0x10);
		u32 nacp_section_offset = *(u32 *)(asset_header + 0x18);
		u32 nacp_section_size = *(u32 *)(asset_header + 0x20);

		// Seek to the icon section offset and read the icon data
		fseek(file, asset_header_offset + icon_section_offset, SEEK_SET);
		data->icon = js_malloc(ctx, icon_section_size);
		if (!data->icon) {
			js_free(ctx, data->nro_path);
			js_free(ctx, data);
			fclose(file);
			return JS_EXCEPTION;
		}
		fread(data->icon, icon_section_size, 1, file);

		// Seek to the nacp section offset and read the nacp data
		fseek(file, asset_header_offset + nacp_section_offset, SEEK_SET);
		fread(&data->nacp, nacp_section_size, 1, file);

		js_free(ctx, asset_header);
		fclose(file);

		data->icon_size = icon_section_size;
		data->is_nro = true;
	} else {
		// Get app ID from parsing the array buffer which contains
		// the contents of an NRO file.
		// NOTE: The file path of the NRO is not known in this case,
		// so the `launch()` method will throw an error.
		size_t nro_size;
		uint8_t *nro = JS_GetArrayBuffer(ctx, &nro_size, argv[0]);
		u32 asset_header_offset = *(u32 *)(nro + 0x18);
		uint8_t *asset_header = nro + asset_header_offset;
		u32 icon_section_offset = *(u32 *)(asset_header + 0x8);
		u32 icon_section_size = *(u32 *)(asset_header + 0x10);
		u32 nacp_section_offset = *(u32 *)(asset_header + 0x18);
		u32 nacp_section_size = *(u32 *)(asset_header + 0x20);
		data->icon = js_mallocz(ctx, icon_section_size);
		if (!data->icon) {
			js_free(ctx, data);
			return JS_EXCEPTION;
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
			return nx_throw_libnx_error(ctx, rc,
										"nsGetApplicationControlData()");
		}
		data->icon_size = outSize > 0 ? outSize - sizeof(buf.nacp) : 0;
		data->icon = js_malloc(ctx, data->icon_size);
		if (!data->icon) {
			js_free(ctx, data);
			return JS_EXCEPTION;
		}
		memcpy(data->icon, &buf.icon, data->icon_size);
		memcpy(&data->nacp, &buf.nacp, sizeof(NacpStruct));
	}

	JSValue app = JS_NewObjectClass(ctx, nx_app_class_id);
	JS_SetOpaque(app, data);
	return app;
}

static JSValue nx_ns_app_id(JSContext *ctx, JSValueConst this_val, int argc,
							JSValueConst *argv) {
	nx_app_t *app = nx_get_app(ctx, this_val);
	if (!app) {
		return JS_EXCEPTION;
	}
	return JS_NewBigUint64(ctx, app->nacp.presence_group_id);
}

static JSValue nx_ns_app_nacp(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	nx_app_t *app = nx_get_app(ctx, this_val);
	if (!app) {
		return JS_EXCEPTION;
	}
	return JS_NewArrayBufferCopy(ctx, (uint8_t *)&app->nacp, sizeof(app->nacp));
}

static JSValue nx_ns_app_icon(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	nx_app_t *app = nx_get_app(ctx, this_val);
	if (!app) {
		return JS_EXCEPTION;
	}
	if (app->icon_size <= 0) {
		return JS_UNDEFINED;
	}
	return JS_NewArrayBufferCopy(ctx, (uint8_t *)app->icon, app->icon_size);
}

static JSValue nx_ns_app_name(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	nx_app_t *app = nx_get_app(ctx, this_val);
	if (!app) {
		return JS_EXCEPTION;
	}
	NacpLanguageEntry *langEntry;
	Result rc = nacpGetLanguageEntry(&app->nacp, &langEntry);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "nacpGetLanguageEntry()");
	}
	if (!langEntry) {
		JS_ThrowPlainError(ctx, "No language entry found");
		return JS_EXCEPTION;
	}
	return JS_NewString(ctx, langEntry->name);
}

static JSValue nx_ns_app_author(JSContext *ctx, JSValueConst this_val, int argc,
								JSValueConst *argv) {
	nx_app_t *app = nx_get_app(ctx, this_val);
	if (!app) {
		return JS_EXCEPTION;
	}
	NacpLanguageEntry *langEntry;
	Result rc = nacpGetLanguageEntry(&app->nacp, &langEntry);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "nacpGetLanguageEntry()");
	}
	if (!langEntry) {
		JS_ThrowPlainError(ctx, "No language entry found");
		return JS_EXCEPTION;
	}
	return JS_NewString(ctx, langEntry->author);
}

static JSValue nx_ns_app_next(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	NsApplicationRecord record;
	int record_count = 0;

	int offset;
	if (JS_ToInt32(ctx, &offset, argv[0])) {
		return JS_EXCEPTION;
	}

	Result rc = nsListApplicationRecord(&record, 1, offset, &record_count);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc, "nsListApplicationRecord()");
	}

	if (!record_count) {
		return JS_NULL;
	}

	return JS_NewBigInt64(ctx, record.application_id);
}

static JSValue nx_ns_app_version(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	nx_app_t *app = nx_get_app(ctx, this_val);
	if (!app) {
		return JS_EXCEPTION;
	}
	return JS_NewString(ctx, app->nacp.display_version);
}

static JSValue nx_ns_app_launch(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	nx_app_t *app = nx_get_app(ctx, this_val);
	if (!app) {
		return JS_EXCEPTION;
	}

	// Prepare arguments for envSetNextLoad
	char **arg_strings = NULL;
	if (argc > 0) {
		if (!app->is_nro) {
			JS_ThrowPlainError(ctx, "Arguments can only be passed to NRO applications");
			return JS_EXCEPTION;
		}

		// Extract arguments directly from argc/argv
		arg_strings = js_malloc(ctx, argc * sizeof(char*));
		if (!arg_strings) {
			return JS_EXCEPTION;
		}

		for (int32_t i = 0; i < argc; i++) {
			const char *str = JS_ToCString(ctx, argv[i]);
			if (!str) {
				// Clean up allocated memory
				for (int32_t j = 0; j < i; j++) {
					js_free(ctx, arg_strings[j]);
				}
				js_free(ctx, arg_strings);
				return JS_EXCEPTION;
			}

			arg_strings[i] = js_strdup(ctx, str);
			if (!arg_strings[i]) {
				JS_FreeCString(ctx, str);
				// Clean up allocated memory
				for (int32_t j = 0; j < i; j++) {
					js_free(ctx, arg_strings[j]);
				}
				js_free(ctx, arg_strings);
				return JS_EXCEPTION;
			}

			JS_FreeCString(ctx, str);
		}
	}

	if (app->is_nro) {
		if (!app->nro_path) {
			JS_ThrowPlainError(ctx, "NRO path not found");
			return JS_EXCEPTION;
		}

		// Ensure the path is prefixed with "sdmc:" for compatibility with hbloader
		const char *launch_path = app->nro_path;
		char *full_path = NULL;
		if (strncmp(app->nro_path, "sdmc:", 5) != 0) {
			size_t path_len = strlen(app->nro_path) + 6; // "sdmc:" + path + null terminator
			full_path = js_malloc(ctx, path_len);
			if (!full_path) {
				return JS_EXCEPTION;
			}
			snprintf(full_path, path_len, "sdmc:%s", app->nro_path);
			launch_path = full_path;
		}

		// Build the argv string with NRO path first, followed by user arguments
		char *args_string = NULL;
		size_t total_length = strlen(launch_path) + 3; // +3 for quotes and null terminator

		// Calculate total length needed including quotes and escaped quotes
		if (arg_strings && argc > 0) {
			for (int32_t i = 0; i < argc; i++) {
				total_length += 3; // space + opening quote + closing quote
				// Count characters that need escaping (double quotes)
				for (const char *p = arg_strings[i]; *p; p++) {
					total_length++;
					if (*p == '"') {
						total_length++; // +1 for escape character
					}
				}
			}
		}

		// Allocate memory for the full argv string
		args_string = js_malloc(ctx, total_length);
		if (!args_string) {
			if (full_path) {
				js_free(ctx, full_path);
			}
			if (arg_strings) {
				for (int32_t i = 0; i < argc; i++) {
					js_free(ctx, arg_strings[i]);
				}
				js_free(ctx, arg_strings);
			}
			return JS_EXCEPTION;
		}

		// Build the argv string: start with quoted NRO path
		snprintf(args_string, total_length, "\"%s\"", launch_path);

		// Append user arguments with proper quoting
		if (arg_strings && argc > 0) {
			for (int32_t i = 0; i < argc; i++) {
				strcat(args_string, " \"");

				// Append the argument with escaped quotes
				size_t current_len = strlen(args_string);
				char *dest = args_string + current_len;
				for (const char *src = arg_strings[i]; *src; src++) {
					if (*src == '"') {
						*dest++ = '\\';
					}
					*dest++ = *src;
				}
				*dest = '\0';

				strcat(args_string, "\"");
			}
		}

		// Configure the next homebrew application to load
		Result rc = envSetNextLoad(launch_path, args_string);

		// Clean up allocated memory
		if (full_path) {
			js_free(ctx, full_path);
		}
		if (args_string) {
			js_free(ctx, args_string);
		}
		if (arg_strings) {
			for (int32_t i = 0; i < argc; i++) {
				js_free(ctx, arg_strings[i]);
			}
			js_free(ctx, arg_strings);
		}

		if (R_FAILED(rc)) {
			return nx_throw_libnx_error(ctx, rc, "envSetNextLoad()");
		}

		// Cleanly exit the event loop to allow the next NRO to load
		__nx_applet_exit_mode = 0;

		nx_exit_event_loop();
	} else {
		Result rc = appletRequestLaunchApplication(app->nacp.presence_group_id, NULL);
		if (R_FAILED(rc)) {
			return nx_throw_libnx_error(ctx, rc, "appletRequestLaunchApplication()");
		}
	}
	return JS_UNDEFINED;
}

static JSValue nx_ns_app_init(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_GET(proto, "id", nx_ns_app_id);
	NX_DEF_GET(proto, "nacp", nx_ns_app_nacp);
	NX_DEF_GET(proto, "icon", nx_ns_app_icon);
	NX_DEF_GET(proto, "name", nx_ns_app_name);
	NX_DEF_GET(proto, "author", nx_ns_app_author);
	NX_DEF_GET(proto, "version", nx_ns_app_version);
	NX_DEF_FUNC(proto, "launch", nx_ns_app_launch, 0);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("nsInitialize", 0, nx_ns_initialize),
	JS_CFUNC_DEF("nsAppNew", 1, nx_ns_app_new),
	JS_CFUNC_DEF("nsAppInit", 1, nx_ns_app_init),
	JS_CFUNC_DEF("nsAppNext", 1, nx_ns_app_next),
};

void nx_init_ns(JSContext *ctx, JSValueConst init_obj) {
	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_app_class_id);
	JSClassDef app_class = {
		"Application",
		.finalizer = finalizer_app,
	};
	JS_NewClass(rt, nx_app_class_id, &app_class);

	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
