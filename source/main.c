#include <errno.h>
#include <inttypes.h>
#include <quickjs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include <unistd.h>

#include "account.h"
#include "album.h"
#include "applet.h"
#include "async.h"
#include "audio.h"
#include "battery.h"
#include "canvas.h"
#include "compression.h"
#include "crypto.h"
#include "dns.h"
#include "dommatrix.h"
#include "error.h"
#include "font.h"
#include "fs.h"
#include "fsdev.h"
#include "gamepad.h"
#include "image.h"
#include "irs.h"
#include "nifm.h"
#include "ns.h"
#include "poll.h"
#include "service.h"
#include "software-keyboard.h"
#include "tcp.h"
#include "tls.h"
#include "types.h"
#include "url.h"
#include "util.h"
#include "wasm.h"
#include "web.h"
#include "window.h"

#define LOG_FILENAME "nxjs-debug.log"

// Defined in runtime.c
extern const uint32_t qjsc_runtime_size;
extern const uint8_t qjsc_runtime[];

// Text renderer
static PrintConsole *print_console = NULL;

// Framebuffer renderer
static NWindow *win = NULL;
static Framebuffer *framebuffer = NULL;
static uint8_t *js_framebuffer = NULL;
static u32 js_fb_width = 0;
static u32 js_fb_height = 0;

void nx_console_init(nx_context_t *nx_ctx) {
	nx_ctx->rendering_mode = NX_RENDERING_MODE_CONSOLE;
	if (print_console == NULL) {
		print_console = consoleInit(NULL);
	}
}

void nx_console_exit() {
	if (print_console != NULL) {
		consoleExit(print_console);
		print_console = NULL;
	}
}

static JSValue nx_framebuffer_init(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	nx_console_exit();
	if (win == NULL) {
		// Retrieve the default window
		win = nwindowGetDefault();
	}
	if (framebuffer != NULL) {
		framebufferClose(framebuffer);
		free(framebuffer);
	}
	u32 width, height;
	nx_canvas_t *canvas = nx_get_canvas(ctx, argv[0]);
	if (!canvas)
		return JS_EXCEPTION;
	width = canvas->width;
	height = canvas->height;
	js_framebuffer = canvas->data;
	js_fb_width = width;
	js_fb_height = height;
	framebuffer = malloc(sizeof(Framebuffer));
	framebufferCreate(framebuffer, win, width, height, PIXEL_FORMAT_BGRA_8888,
					  2);
	framebufferMakeLinear(framebuffer);
	nx_ctx->rendering_mode = NX_RENDERING_MODE_CANVAS;
	return JS_UNDEFINED;
}

void nx_framebuffer_exit() {
	if (framebuffer != NULL) {
		framebufferClose(framebuffer);
		free(framebuffer);
		framebuffer = NULL;
		js_framebuffer = NULL;
	}
}

uint8_t *read_file(const char *filename, size_t *out_size) {
	FILE *file = fopen(filename, "rb");
	if (file == NULL) {
		return NULL;
	}

	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	rewind(file);

	uint8_t *buffer = malloc(size + 1);
	if (buffer == NULL) {
		fclose(file);
		return NULL;
	}

	size_t result = fread(buffer, 1, size, file);
	fclose(file);

	if (result != size) {
		free(buffer);
		return NULL;
	}

	*out_size = size;

	// NULL terminate the buffer, to work around bug in QuickJS
	// eval where doesn't respect the provided buffer size
	buffer[size] = '\0';

	return buffer;
}

bool delete_if_empty(const char *filename) {
	FILE *file = fopen(filename, "rb");
	if (!file) {
		return false;
	}

	// Seek to the end of the file
	fseek(file, 0, SEEK_END);

	// Get the file size
	long size = ftell(file);

	// Close the file
	fclose(file);

	// If the file is empty, delete it
	if (size == 0) {
		if (remove(filename) == 0) {
			return true;
		} else {
			return false;
		}
	}

	return true;
}

static int is_running = 1;

static JSValue js_exit(JSContext *ctx, JSValueConst this_val, int argc,
					   JSValueConst *argv) {
	is_running = 0;
	return JS_UNDEFINED;
}

// Function to cleanly exit the main event loop (for use by other modules)
void nx_exit_event_loop(void) {
	is_running = 0;
}

static JSValue js_print(JSContext *ctx, JSValueConst this_val, int argc,
						JSValueConst *argv) {
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	if (nx_ctx->rendering_mode != NX_RENDERING_MODE_CONSOLE) {
		nx_framebuffer_exit();
		nx_console_init(nx_ctx);
	}
	const char *str = JS_ToCString(ctx, argv[0]);
	printf("%s", str);
	JS_FreeCString(ctx, str);
	return JS_UNDEFINED;
}

static JSValue js_print_err(JSContext *ctx, JSValueConst this_val, int argc,
							JSValueConst *argv) {
	const char *str = JS_ToCString(ctx, argv[0]);
	fprintf(stderr, "%s", str);
	JS_FreeCString(ctx, str);
	return JS_UNDEFINED;
}

static JSValue js_cwd(JSContext *ctx, JSValueConst this_val, int argc,
					  JSValueConst *argv) {
	char cwd[1024];
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		char final_path[1029];

		// Emulators such as Ryujinx don't have
		// the "sdmc:" prefix, so add it
		if (cwd[0] == '/') {
			snprintf(final_path, sizeof(final_path), "sdmc:%s", cwd);
		} else {
			snprintf(final_path, sizeof(final_path), "%s", cwd);
		}

		// Ensure the path ends with a trailing slash
		// so that it works nicely with `new URL()`
		size_t len = strlen(final_path);
		if (len > 0 && final_path[len - 1] != '/') {
			final_path[len] = '/';
			final_path[len + 1] = '\0';
		}

		return JS_NewString(ctx, final_path);
	}
	return JS_UNDEFINED;
}

static JSValue js_chdir(JSContext *ctx, JSValueConst this_val, int argc,
						JSValueConst *argv) {
	const char *dir = JS_ToCString(ctx, argv[0]);
	if (chdir(dir) != 0) {
		JS_ThrowTypeError(ctx, "%s: %s", strerror(errno), dir);
		JS_FreeCString(ctx, dir);
		return JS_EXCEPTION;
	}
	JS_FreeCString(ctx, dir);
	return JS_UNDEFINED;
}

static JSValue js_hid_initialize_touch_screen(JSContext *ctx,
											  JSValueConst this_val, int argc,
											  JSValueConst *argv) {
	hidInitializeTouchScreen();
	return JS_UNDEFINED;
}

static JSValue js_hid_initialize_keyboard(JSContext *ctx, JSValueConst this_val,
										  int argc, JSValueConst *argv) {
	hidInitializeKeyboard();
	return JS_UNDEFINED;
}

static JSValue js_hid_initialize_vibration_devices(JSContext *ctx,
												   JSValueConst this_val,
												   int argc,
												   JSValueConst *argv) {
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	Result rc = hidInitializeVibrationDevices(
		nx_ctx->vibration_device_handles, 2,
		// TODO: handle No1 gamepad
		HidNpadIdType_Handheld, HidNpadStyleSet_NpadStandard);
	if (R_FAILED(rc)) {
		JS_ThrowInternalError(
			ctx, "hidInitializeVibrationDevices() returned 0x%x", rc);
		return JS_EXCEPTION;
	}
	return JS_UNDEFINED;
}

static JSValue js_hid_send_vibration_values(JSContext *ctx,
											JSValueConst this_val, int argc,
											JSValueConst *argv) {
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	HidVibrationValue VibrationValues[2];
	JSValue low_amp_value = JS_GetPropertyStr(ctx, argv[0], "lowAmp");
	JSValue low_freq_value = JS_GetPropertyStr(ctx, argv[0], "lowFreq");
	JSValue high_amp_value = JS_GetPropertyStr(ctx, argv[0], "highAmp");
	JSValue high_freq_value = JS_GetPropertyStr(ctx, argv[0], "highFreq");
	double low_amp, low_freq, high_amp, high_freq;
	int err = JS_ToFloat64(ctx, &low_amp, low_amp_value) ||
			  JS_ToFloat64(ctx, &low_freq, low_freq_value) ||
			  JS_ToFloat64(ctx, &high_amp, high_amp_value) ||
			  JS_ToFloat64(ctx, &high_freq, high_freq_value);
	JS_FreeValue(ctx, low_amp_value);
	JS_FreeValue(ctx, low_freq_value);
	JS_FreeValue(ctx, high_amp_value);
	JS_FreeValue(ctx, high_freq_value);
	if (err) {
		return JS_EXCEPTION;
	}
	VibrationValues[0].freq_low = low_freq;
	VibrationValues[0].amp_low = low_amp;
	VibrationValues[0].freq_high = high_freq;
	VibrationValues[0].amp_high = high_amp;
	memcpy(&VibrationValues[1], &VibrationValues[0], sizeof(HidVibrationValue));

	Result rc = hidSendVibrationValues(nx_ctx->vibration_device_handles,
									   VibrationValues, 2);
	if (R_FAILED(rc)) {
		JS_ThrowInternalError(ctx, "hidSendVibrationValues() returned 0x%x",
							  rc);
		return JS_EXCEPTION;
	}
	return JS_UNDEFINED;
}

static JSValue js_hid_get_touch_screen_states(JSContext *ctx,
											  JSValueConst this_val, int argc,
											  JSValueConst *argv) {
	HidTouchScreenState state = {0};
	hidGetTouchScreenStates(&state, 1);
	if (state.count == 0) {
		return JS_UNDEFINED;
	}
	JSValue arr = JS_NewArray(ctx);
	for (int i = 0; i < state.count; i++) {
		JSValue touch = JS_NewObject(ctx);
		JSValue x = JS_NewInt32(ctx, state.touches[i].x);
		JSValue y = JS_NewInt32(ctx, state.touches[i].y);
		JS_SetPropertyUint32(ctx, arr, i, touch);
		JS_SetPropertyStr(ctx, touch, "identifier",
						  JS_NewInt32(ctx, state.touches[i].finger_id));
		JS_SetPropertyStr(ctx, touch, "clientX", x);
		JS_SetPropertyStr(ctx, touch, "clientY", y);
		JS_SetPropertyStr(ctx, touch, "screenX", x);
		JS_SetPropertyStr(ctx, touch, "screenY", y);
		JS_SetPropertyStr(
			ctx, touch, "radiusX",
			JS_NewFloat64(ctx, (double)state.touches[i].diameter_x / 2.0));
		JS_SetPropertyStr(
			ctx, touch, "radiusY",
			JS_NewFloat64(ctx, (double)state.touches[i].diameter_y / 2.0));
		JS_SetPropertyStr(ctx, touch, "rotationAngle",
						  JS_NewInt32(ctx, state.touches[i].rotation_angle));
	}
	return arr;
}

static JSValue js_hid_get_keyboard_states(JSContext *ctx, JSValueConst this_val,
										  int argc, JSValueConst *argv) {
	HidKeyboardState state = {0};
	hidGetKeyboardStates(&state, 1);
	JSValue obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, obj, "modifiers",
					  JS_NewBigUint64(ctx, state.modifiers));
	for (int i = 0; i < 4; i++) {
		JS_SetPropertyUint32(ctx, obj, i, JS_NewBigUint64(ctx, state.keys[i]));
	}
	return obj;
}

static JSValue js_getenv(JSContext *ctx, JSValueConst this_val, int argc,
						 JSValueConst *argv) {
	const char *name = JS_ToCString(ctx, argv[0]);
	char *value = getenv(name);
	if (value == NULL) {
		if (errno) {
			JS_ThrowTypeError(ctx, "%s: %s", strerror(errno), name);
			JS_FreeCString(ctx, name);
			return JS_EXCEPTION;
		}
		JS_FreeCString(ctx, name);
		return JS_UNDEFINED;
	}
	JS_FreeCString(ctx, name);
	return JS_NewString(ctx, value);
}

static JSValue js_setenv(JSContext *ctx, JSValueConst this_val, int argc,
						 JSValueConst *argv) {
	const char *name = JS_ToCString(ctx, argv[0]);
	const char *value = JS_ToCString(ctx, argv[1]);
	if (setenv(name, value, 1) != 0) {
		JS_ThrowTypeError(ctx, "%s: %s=%s", strerror(errno), name, value);
		JS_FreeCString(ctx, name);
		JS_FreeCString(ctx, value);
		return JS_EXCEPTION;
	}
	JS_FreeCString(ctx, name);
	JS_FreeCString(ctx, value);
	return JS_UNDEFINED;
}

static JSValue js_unsetenv(JSContext *ctx, JSValueConst this_val, int argc,
						   JSValueConst *argv) {
	const char *name = JS_ToCString(ctx, argv[0]);
	if (unsetenv(name) != 0) {
		JS_ThrowTypeError(ctx, "%s: %s", strerror(errno), name);
		JS_FreeCString(ctx, name);
		return JS_EXCEPTION;
	}
	JS_FreeCString(ctx, name);
	return JS_UNDEFINED;
}

static JSValue js_env_to_object(JSContext *ctx, JSValueConst this_val, int argc,
								JSValueConst *argv) {
	JSValue env = JS_NewObject(ctx);

	// Get the environment variables from the operating system
	extern char **environ;
	char **envp = environ;
	while (*envp) {
		// Split each environment variable into a key-value pair
		char *key = strdup(*envp);
		char *eq = strchr(key, '=');
		if (eq) {
			*eq = '\0';
			char *value = eq + 1;

			JS_SetPropertyStr(ctx, env, key, JS_NewString(ctx, value));
		}
		free(key);
		envp++;
	}

	return env;
}

// Returns the internal state of a Promise instance.
static JSValue js_get_internal_promise_state(JSContext *ctx,
											 JSValueConst this_val, int argc,
											 JSValueConst *argv) {
	JSPromiseStateEnum state = JS_PromiseState(ctx, argv[0]);
	JSValue arr = JS_NewArray(ctx);
	JS_SetPropertyUint32(ctx, arr, 0, JS_NewUint32(ctx, state));
	if (state > JS_PROMISE_PENDING) {
		JS_SetPropertyUint32(ctx, arr, 1, JS_PromiseResult(ctx, argv[0]));
	} else {
		JS_SetPropertyUint32(ctx, arr, 1, JS_NULL);
	}
	return arr;
}

static JSValue nx_set_frame_handler(JSContext *ctx, JSValueConst this_val,
									int argc, JSValueConst *argv) {
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	nx_ctx->frame_handler = JS_DupValue(ctx, argv[0]);
	return JS_UNDEFINED;
}

static JSValue nx_set_exit_handler(JSContext *ctx, JSValueConst this_val,
								   int argc, JSValueConst *argv) {
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	nx_ctx->exit_handler = JS_DupValue(ctx, argv[0]);
	return JS_UNDEFINED;
}

static JSValue nx_version_get_ams(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	if (!hosversionIsAtmosphere()) {
		return JS_UNDEFINED;
	}

	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	if (!nx_ctx->spl_initialized) {
		nx_ctx->spl_initialized = true;
		splInitialize();
	}

	u64 packed_version;
	Result rc = splGetConfig((SplConfigItem)65000, &packed_version);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc,
									"splGetConfig(ExosphereApiVersion)");
	}
	u8 major_version = (packed_version >> 56) & 0xFF;
	u8 minor_version = (packed_version >> 48) & 0xFF;
	u8 micro_version = (packed_version >> 40) & 0xFF;
	char version_str[12];
	snprintf(version_str, 12, "%u.%u.%u", major_version, minor_version,
			 micro_version);
	return JS_NewString(ctx, version_str);
}

static JSValue nx_version_get_emummc(JSContext *ctx, JSValueConst this_val,
									 int argc, JSValueConst *argv) {
	if (!hosversionIsAtmosphere()) {
		return JS_UNDEFINED;
	}

	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	if (!nx_ctx->spl_initialized) {
		nx_ctx->spl_initialized = true;
		splInitialize();
	}

	u64 is_emummc;
	Result rc = splGetConfig((SplConfigItem)65007, &is_emummc);
	if (R_FAILED(rc)) {
		return nx_throw_libnx_error(ctx, rc,
									"splGetConfig(ExosphereEmummcType)");
	}
	return JS_NewBool(ctx, is_emummc ? true : false);
}

int nx_module_set_import_meta(JSContext *ctx, JSValueConst func_val,
							  const char *url, bool is_main) {
	JSModuleDef *m = JS_VALUE_GET_PTR(func_val);
	JSValue meta_obj = JS_GetImportMeta(ctx, m);
	if (JS_IsException(meta_obj))
		return -1;
	JS_DefinePropertyValueStr(ctx, meta_obj, "url", JS_NewString(ctx, url),
							  JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, meta_obj, "main", JS_NewBool(ctx, is_main),
							  JS_PROP_C_W_E);
	JS_FreeValue(ctx, meta_obj);
	return 0;
}

void nx_process_pending_jobs(JSContext *ctx, nx_context_t *nx_ctx,
							 JSRuntime *rt) {
	JSContext *ctx1;
	int err;
	// Don't allow an infinite number of pending jobs
	// to allow the UI to update periodically. The number
	// of iterations was chosen arbitrarily - maybe could
	// be optimized by using a timer instead of a fixed
	// number of iterations.
	for (u8 i = 0; i < 20; i++) {
		err = JS_ExecutePendingJob(rt, &ctx1);
		if (err <= 0) {
			if (err < 0) {
				nx_emit_error_event(ctx1);
			}
			break;
		}
	}

	if (!JS_IsUndefined(nx_ctx->unhandled_rejected_promise)) {
		nx_emit_unhandled_rejection_event(ctx);
	}
}

void nx_render_loading_image(nx_context_t *nx_ctx, const char *nro_path) {
	// Check if there is a `loading.jpg` file on the RomFS
	// and render that to the screen if present.
	size_t loading_image_size;
	const char *loading_image_path = "romfs:/loading.jpg";
	uint8_t *loading_image = (uint8_t *)read_file(loading_image_path, &loading_image_size);
	if (!loading_image && nro_path) {
		// RomFS loading_image image not found.
		// Try to load from SD card, relative to the path of the NRO.
		char *temp_loading_image_path = strdup(nro_path);
		if (temp_loading_image_path) {
			replace_file_extension(temp_loading_image_path, ".jpg");
			loading_image = (uint8_t *)read_file(temp_loading_image_path, &loading_image_size);
			free(temp_loading_image_path);
		}
	}
	if (loading_image != NULL) {
		win = nwindowGetDefault();
		int width = 1280;
		int height = 720;
		js_framebuffer = malloc(width * height * 4);
		framebuffer = malloc(sizeof(Framebuffer));
		framebufferCreate(framebuffer, win, width, height, PIXEL_FORMAT_BGRA_8888,
						2);
		framebufferMakeLinear(framebuffer);

		decode_jpeg(loading_image, loading_image_size, &js_framebuffer, &width, &height);
		// TODO: ensure decompression was successful
		// TODO: ensure width and height are correct

		u32 stride;
		u8 *framebuf = (u8 *)framebufferBegin(framebuffer, &stride);
		memcpy(framebuf, js_framebuffer, width * height * 4);
		framebufferEnd(framebuffer);

		free(js_framebuffer);
		free(loading_image);
		js_framebuffer = NULL;
	}
}

static SocketInitConfig const s_socketInitConfig = {
	.tcp_tx_buf_size = 1 * 1024 * 1024,
	.tcp_rx_buf_size = 1 * 1024 * 1024,
	.tcp_tx_buf_max_size = 4 * 1024 * 1024,
	.tcp_rx_buf_max_size = 4 * 1024 * 1024,

	.udp_tx_buf_size = 0x2400,
	.udp_rx_buf_size = 0xA500,

	.sb_efficiency = 8,

	.num_bsd_sessions = 3,
	.bsd_service_type = BsdServiceType_Auto,
};

// Main program entrypoint
int main(int argc, char *argv[]) {
	Result rc;

	nx_context_t *nx_ctx = malloc(sizeof(nx_context_t));
	memset(nx_ctx, 0, sizeof(nx_context_t));

	rc = romfsInit();
	if (R_FAILED(rc)) {
		diagAbortWithResult(rc);
	}

	nx_render_loading_image(nx_ctx, argc > 0 ? argv[0] : NULL);

	rc = socketInitialize(&s_socketInitConfig);
	if (R_FAILED(rc)) {
		diagAbortWithResult(rc);
	}

	rc = plInitialize(PlServiceType_User);
	if (R_FAILED(rc)) {
		diagAbortWithResult(rc);
	}

	FILE *debug_fd = freopen(LOG_FILENAME, "w", stderr);

	JSRuntime *rt = JS_NewRuntime();
	JSContext *ctx = JS_NewContext(rt);

	nx_ctx->rendering_mode = NX_RENDERING_MODE_INIT;
	nx_ctx->thpool = thpool_init(4);
	nx_ctx->frame_handler = JS_UNDEFINED;
	nx_ctx->exit_handler = JS_UNDEFINED;
	nx_ctx->error_handler = JS_UNDEFINED;
	nx_ctx->unhandled_rejection_handler = JS_UNDEFINED;
	nx_ctx->unhandled_rejected_promise = JS_UNDEFINED;
	pthread_mutex_init(&(nx_ctx->async_done_mutex), NULL);
	JS_SetContextOpaque(ctx, nx_ctx);
	JS_SetRuntimeOpaque(rt, nx_ctx);
	JS_SetHostPromiseRejectionTracker(rt, nx_promise_rejection_handler, ctx);

	padConfigureInput(8, HidNpadStyleSet_NpadStandard | HidNpadStyleTag_NpadGc);
	padInitializeDefault(&nx_ctx->pads[0]);
	padInitialize(&nx_ctx->pads[1], HidNpadIdType_No2);
	padInitialize(&nx_ctx->pads[2], HidNpadIdType_No3);
	padInitialize(&nx_ctx->pads[3], HidNpadIdType_No4);
	padInitialize(&nx_ctx->pads[4], HidNpadIdType_No5);
	padInitialize(&nx_ctx->pads[5], HidNpadIdType_No6);
	padInitialize(&nx_ctx->pads[6], HidNpadIdType_No7);
	padInitialize(&nx_ctx->pads[7], HidNpadIdType_No8);

	// First try the `main.jsc` file on the RomFS, which should
	// contain the bytecode entrypoint compiled with `qjsc -b`
	size_t user_code_size;
	bool user_path_needs_free = false;
	bool user_code_is_bytecode = true;
	char *user_code_path = "romfs:/main.jsc";
	char *user_code = (char *)read_file(user_code_path, &user_code_size);

	if (user_code == NULL && errno == ENOENT) {
		// If no `main.jsc`, then try the `main.js` file on the RomFS
		user_code_is_bytecode = false;
		user_code_path = "romfs:/main.js";
		user_code = (char *)read_file(user_code_path, &user_code_size);
	}

	if (user_code == NULL && errno == ENOENT && argc > 0) {
		// If no `main.js`, then try the `.js` file with the matching name
		// as the `.nro` file on the SD card
		user_path_needs_free = true;
		user_code_path = strdup(argv[0]);
		if (user_code_path) {
			replace_file_extension(user_code_path, ".js");
			user_code = (char *)read_file(user_code_path, &user_code_size);
		}
	}

	if (user_code == NULL) {
		nx_console_init(nx_ctx);
		printf("%s: %s\n", strerror(errno), user_code_path);
		if (user_path_needs_free) {
			free(user_code_path);
		}
		nx_ctx->had_error = 1;
		goto main_loop;
	}

	// The internal `$` object contains native functions that are wrapped in the
	// JS runtime
	JSValue global_obj = JS_GetGlobalObject(ctx);
	nx_ctx->init_obj = JS_NewObject(ctx);
	nx_init_account(ctx, nx_ctx->init_obj);
	nx_init_album(ctx, nx_ctx->init_obj);
	nx_init_applet(ctx, nx_ctx->init_obj);
	nx_init_audio(ctx, nx_ctx->init_obj);
	nx_init_battery(ctx, nx_ctx->init_obj);
	nx_init_canvas(ctx, nx_ctx->init_obj);
	nx_init_compression(ctx, nx_ctx->init_obj);
	nx_init_crypto(ctx, nx_ctx->init_obj);
	nx_init_dns(ctx, nx_ctx->init_obj);
	nx_init_dommatrix(ctx, nx_ctx->init_obj);
	nx_init_error(ctx, nx_ctx->init_obj);
	nx_init_font(ctx, nx_ctx->init_obj);
	nx_init_fs(ctx, nx_ctx->init_obj);
	nx_init_fsdev(ctx, nx_ctx->init_obj);
	nx_init_gamepad(ctx, nx_ctx->init_obj);
	nx_init_image(ctx, nx_ctx->init_obj);
	nx_init_irs(ctx, nx_ctx->init_obj);
	nx_init_nifm(ctx, nx_ctx->init_obj);
	nx_init_ns(ctx, nx_ctx->init_obj);
	nx_init_service(ctx, nx_ctx->init_obj);
	nx_init_tcp(ctx, nx_ctx->init_obj);
	nx_init_tls(ctx, nx_ctx->init_obj);
	nx_init_url(ctx, nx_ctx->init_obj);
	nx_init_swkbd(ctx, nx_ctx->init_obj);
	nx_init_wasm(ctx, nx_ctx->init_obj);
	nx_init_web(ctx, nx_ctx->init_obj);
	nx_init_window(ctx, nx_ctx->init_obj);
	const JSCFunctionListEntry init_function_list[] = {
		JS_CFUNC_DEF("exit", 0, js_exit),
		JS_CFUNC_DEF("cwd", 0, js_cwd),
		JS_CFUNC_DEF("chdir", 1, js_chdir),
		JS_CFUNC_DEF("print", 1, js_print),
		JS_CFUNC_DEF("printErr", 1, js_print_err),
		JS_CFUNC_DEF("getInternalPromiseState", 1,
					 js_get_internal_promise_state),
		JS_CFUNC_DEF("hidInitializeTouchScreen", 0,
					 js_hid_initialize_touch_screen),
		JS_CFUNC_DEF("hidGetTouchScreenStates", 0,
					 js_hid_get_touch_screen_states),

		// env vars
		JS_CFUNC_DEF("getenv", 1, js_getenv),
		JS_CFUNC_DEF("setenv", 2, js_setenv),
		JS_CFUNC_DEF("unsetenv", 1, js_unsetenv),
		JS_CFUNC_DEF("envToObject", 0, js_env_to_object),

		JS_CFUNC_DEF("onExit", 1, nx_set_exit_handler),
		JS_CFUNC_DEF("onFrame", 1, nx_set_frame_handler),

		// framebuffer renderer
		JS_CFUNC_DEF("framebufferInit", 1, nx_framebuffer_init),

		// hid
		JS_CFUNC_DEF("hidInitializeKeyboard", 0, js_hid_initialize_keyboard),
		JS_CFUNC_DEF("hidInitializeVibrationDevices", 0,
					 js_hid_initialize_vibration_devices),
		JS_CFUNC_DEF("hidGetKeyboardStates", 0, js_hid_get_keyboard_states),
		JS_CFUNC_DEF("hidSendVibrationValues", 0, js_hid_send_vibration_values),
	};
	JS_SetPropertyFunctionList(ctx, nx_ctx->init_obj, init_function_list,
							   countof(init_function_list));

	// `Switch.version`
	JSAtom atom;
	char version_str[12];
	JSValue version_obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, version_obj, "ada", JS_NewString(ctx, "2.9.2"));
	NX_DEF_GET_(version_obj, "ams", nx_version_get_ams, JS_PROP_C_W_E);
	JS_SetPropertyStr(ctx, version_obj, "cairo",
					  JS_NewString(ctx, cairo_version_string()));
	NX_DEF_GET_(version_obj, "emummc", nx_version_get_emummc, JS_PROP_C_W_E);
	JS_SetPropertyStr(ctx, version_obj, "freetype2",
					  JS_NewString(ctx, FREETYPE_VERSION_STR));
	JS_SetPropertyStr(ctx, version_obj, "harfbuzz",
					  JS_NewString(ctx, HB_VERSION_STRING));
	u32 hos_version = hosversionGet();
	snprintf(version_str, 12, "%d.%d.%d", HOSVER_MAJOR(hos_version),
			 HOSVER_MINOR(hos_version), HOSVER_MICRO(hos_version));
	JS_SetPropertyStr(ctx, version_obj, "hos", JS_NewString(ctx, version_str));
	JS_SetPropertyStr(ctx, version_obj, "libnx",
					  JS_NewString(ctx, LIBNX_VERSION));
	JS_SetPropertyStr(ctx, version_obj, "mbedtls",
					  JS_NewString(ctx, MBEDTLS_VERSION_STRING));
	JS_SetPropertyStr(ctx, version_obj, "nxjs",
					  JS_NewString(ctx, NXJS_VERSION));
	JS_SetPropertyStr(ctx, version_obj, "png",
					  JS_NewString(ctx, PNG_LIBPNG_VER_STRING));
	JS_SetPropertyStr(ctx, version_obj, "quickjs",
					  JS_NewString(ctx, JS_GetVersion()));
	JS_SetPropertyStr(ctx, version_obj, "turbojpeg",
					  JS_NewString(ctx, LIBTURBOJPEG_VERSION));
	JS_SetPropertyStr(ctx, version_obj, "wasm3", JS_NewString(ctx, M3_VERSION));
	const int webp_version = WebPGetDecoderVersion();
	snprintf(version_str, 12, "%d.%d.%d", (webp_version >> 16) & 0xFF,
			 (webp_version >> 8) & 0xFF, webp_version & 0xFF);
	JS_SetPropertyStr(ctx, version_obj, "webp", JS_NewString(ctx, version_str));
	JS_SetPropertyStr(ctx, version_obj, "zlib",
					  JS_NewString(ctx, zlibVersion()));
	JS_SetPropertyStr(ctx, version_obj, "zstd",
					  JS_NewString(ctx, ZSTD_versionString()));
	JS_SetPropertyStr(ctx, nx_ctx->init_obj, "version", version_obj);

	// `Switch.entrypoint`
	JS_SetPropertyStr(ctx, nx_ctx->init_obj, "entrypoint",
					  JS_NewString(ctx, user_code_path));

	// `Switch.argv`
	JSValue argv_array = JS_NewArray(ctx);
	for (int i = 0; i < argc; i++) {
		JS_SetPropertyUint32(ctx, argv_array, i, JS_NewString(ctx, argv[i]));
	}
	JS_SetPropertyStr(ctx, nx_ctx->init_obj, "argv", argv_array);

	JS_SetPropertyStr(ctx, global_obj, "$", nx_ctx->init_obj);

	// Initialize runtime
	JSValue runtime_init_func, runtime_init_result;
	runtime_init_func = JS_ReadObject(ctx, qjsc_runtime, qjsc_runtime_size,
									  JS_READ_OBJ_BYTECODE);
	if (JS_IsException(runtime_init_func)) {
		print_js_error(ctx);
		nx_ctx->had_error = 1;
		goto main_loop;
	}
	runtime_init_result = JS_EvalFunction(ctx, runtime_init_func);
	if (JS_IsException(runtime_init_result)) {
		nx_console_init(nx_ctx);
		printf("Runtime initialization failed\n");
		print_js_error(ctx);
		nx_ctx->had_error = 1;
		goto main_loop;
	}
	JS_FreeValue(ctx, runtime_init_result);

	// Run the user code
	JSValue user_code_result;
	if (user_code_is_bytecode) {
		user_code_result = JS_ReadObject(ctx, (const u8 *)user_code,
										 user_code_size, JS_READ_OBJ_BYTECODE);
		if (JS_IsException(user_code_result)) {
			nx_emit_error_event(ctx);
		} else {
			nx_module_set_import_meta(ctx, user_code_result, user_code_path,
									  true);
			user_code_result = JS_EvalFunction(ctx, user_code_result);
			if (JS_IsException(user_code_result)) {
				nx_emit_error_event(ctx);
			}
		}
	} else {
		user_code_result =
			JS_Eval(ctx, user_code, user_code_size, user_code_path,
					JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
		if (JS_IsException(user_code_result)) {
			nx_emit_error_event(ctx);
		} else {
			nx_module_set_import_meta(ctx, user_code_result, user_code_path,
									  true);
			user_code_result = JS_EvalFunction(ctx, user_code_result);
			if (JS_IsException(user_code_result)) {
				nx_emit_error_event(ctx);
			}
		}
	}
	JS_FreeValue(ctx, user_code_result);
	free(user_code);
	if (user_path_needs_free) {
		free(user_code_path);
	}

main_loop:
	while (appletMainLoop()) {
		if (!nx_ctx->had_error) {
			// Check if any file descriptors have reported activity
			nx_poll(&nx_ctx->poll);
		}

		if (!nx_ctx->had_error) {
			// Check if any thread pool tasks have completed
			nx_process_async(ctx, nx_ctx);
		}

		if (!nx_ctx->had_error) {
			// Process any Promises that need to be fulfilled
			nx_process_pending_jobs(ctx, nx_ctx, rt);
		}

		// Update controller pad states
		padUpdate(&nx_ctx->pads[0]);
		padUpdate(&nx_ctx->pads[1]);
		padUpdate(&nx_ctx->pads[2]);
		padUpdate(&nx_ctx->pads[3]);
		padUpdate(&nx_ctx->pads[4]);
		padUpdate(&nx_ctx->pads[5]);
		padUpdate(&nx_ctx->pads[6]);
		padUpdate(&nx_ctx->pads[7]);

		u64 kDown = padGetButtonsDown(&nx_ctx->pads[0]);
		bool plusDown = kDown & HidNpadButton_Plus;

		if (nx_ctx->had_error) {
			if (plusDown) {
				// When an initialization or unhandled error occurs,
				// wait until the user presses "+" to fully exit so
				// the user has a chance to read the error message.
				break;
			}
		} else {
			// Call frame handler
			JSValueConst args[] = {JS_NewBool(ctx, plusDown)};
			JSValue ret_val =
				JS_Call(ctx, nx_ctx->frame_handler, JS_NULL, 1, args);

			if (JS_IsException(ret_val)) {
				nx_emit_error_event(ctx);
			}
			JS_FreeValue(ctx, ret_val);

			if (!is_running) {
				// `Switch.exit()` was called
				break;
			}
		}

		if (nx_ctx->rendering_mode == NX_RENDERING_MODE_CONSOLE) {
			// Update the console, sending a new frame to the display
			consoleUpdate(print_console);
		} else if (nx_ctx->rendering_mode == NX_RENDERING_MODE_CANVAS) {
			// Copy the JS framebuffer to the current Switch buffer
			u32 stride;
			u8 *framebuf = (u8 *)framebufferBegin(framebuffer, &stride);
			memcpy(framebuf, js_framebuffer, js_fb_width * js_fb_height * 4);
			framebufferEnd(framebuffer);
		}
	}

	// XXX: Ideally we wouldn't `thpool_wait()` here,
	// but the app seems to crash without it
	thpool_wait(nx_ctx->thpool);
	thpool_destroy(nx_ctx->thpool);

	// Call exit handler
	JSValue ret_val = JS_Call(ctx, nx_ctx->exit_handler, JS_NULL, 0, NULL);
	JS_FreeValue(ctx, ret_val);

	if (nx_ctx->rendering_mode == NX_RENDERING_MODE_CONSOLE) {
		nx_console_exit();
	} else if (nx_ctx->rendering_mode == NX_RENDERING_MODE_CANVAS) {
		nx_framebuffer_exit();
	}

	fclose(debug_fd);
	FILE *leaks_fd = freopen(LOG_FILENAME, "a", stdout);

	JS_FreeValue(ctx, global_obj);
	JS_FreeValue(ctx, nx_ctx->frame_handler);
	JS_FreeValue(ctx, nx_ctx->exit_handler);
	JS_FreeValue(ctx, nx_ctx->error_handler);
	JS_FreeValue(ctx, nx_ctx->unhandled_rejection_handler);

	JS_FreeContext(ctx);
	JS_FreeRuntime(rt);

	if (nx_ctx->wasm_env) {
		m3_FreeEnvironment(nx_ctx->wasm_env);
	}
	if (nx_ctx->ft_library) {
		FT_Done_FreeType(nx_ctx->ft_library);
	}

	if (nx_ctx->mbedtls_initialized) {
		mbedtls_ctr_drbg_free(&nx_ctx->ctr_drbg);
		mbedtls_entropy_free(&nx_ctx->entropy);
	}
	if (nx_ctx->ca_certs_loaded) {
		mbedtls_x509_crt_free(&nx_ctx->ca_chain);
	}

	if (nx_ctx->spl_initialized) {
		splExit();
	}

	free(nx_ctx);

	plExit();
	romfsExit();
	socketExit();

	fflush(leaks_fd);
	fclose(leaks_fd);

	/* If nothing was written to the debug log file, then delete it */
	delete_if_empty(LOG_FILENAME);

	return 0;
}
