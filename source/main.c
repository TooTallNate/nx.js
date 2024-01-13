#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <switch.h>
#include <quickjs.h>

#include "types.h"
#include "account.h"
#include "applet.h"
#include "async.h"
#include "battery.h"
#include "crypto.h"
#include "dns.h"
#include "error.h"
#include "canvas.h"
#include "font.h"
#include "fs.h"
#include "fsdev.h"
#include "irs.h"
#include "nifm.h"
#include "ns.h"
#include "software-keyboard.h"
#include "wasm.h"
#include "image.h"
#include "tcp.h"
#include "tls.h"
#include "poll.h"

#define LOG_FILENAME "nxjs-debug.log"

// Text renderer
static PrintConsole *print_console = NULL;

// Framebuffer renderer
static NWindow *win = NULL;
static Framebuffer *framebuffer = NULL;
static uint8_t *js_framebuffer = NULL;

void nx_console_exit()
{
	if (print_console != NULL)
	{
		consoleExit(print_console);
		print_console = NULL;
	}
}

static JSValue nx_framebuffer_init(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	nx_console_exit();
	if (win == NULL)
	{
		// Retrieve the default window
		win = nwindowGetDefault();
	}
	if (framebuffer != NULL)
	{
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
	framebuffer = malloc(sizeof(Framebuffer));
	framebufferCreate(framebuffer, win, width, height, PIXEL_FORMAT_BGRA_8888, 2);
	framebufferMakeLinear(framebuffer);
	nx_ctx->rendering_mode = NX_RENDERING_MODE_CANVAS;
	return JS_UNDEFINED;
}

void nx_framebuffer_exit()
{
	if (framebuffer != NULL)
	{
		framebufferClose(framebuffer);
		free(framebuffer);
		framebuffer = NULL;
		js_framebuffer = NULL;
	}
}

uint8_t *read_file(const char *filename, size_t *out_size)
{
	FILE *file = fopen(filename, "rb");
	if (file == NULL)
	{
		return NULL;
	}

	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	rewind(file);

	uint8_t *buffer = malloc(size + 1);
	if (buffer == NULL)
	{
		fclose(file);
		return NULL;
	}

	size_t result = fread(buffer, 1, size, file);
	fclose(file);

	if (result != size)
	{
		free(buffer);
		return NULL;
	}

	*out_size = size;

	// NULL terminate the buffer, to work around bug in QuickJS
	// eval where doesn't respect the provided buffer size
	buffer[size] = '\0';

	return buffer;
}

bool delete_if_empty(const char *filename)
{
	FILE *file = fopen(filename, "rb");
	if (!file)
	{
		return false;
	}

	// Seek to the end of the file
	fseek(file, 0, SEEK_END);

	// Get the file size
	long size = ftell(file);

	// Close the file
	fclose(file);

	// If the file is empty, delete it
	if (size == 0)
	{
		if (remove(filename) == 0)
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	return true;
}

static int is_running = 1;

static JSValue js_exit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	is_running = 0;
	return JS_UNDEFINED;
}

static JSValue js_print(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	if (nx_ctx->rendering_mode != NX_RENDERING_MODE_CONSOLE)
	{
		nx_framebuffer_exit();
		if (print_console == NULL)
		{
			print_console = consoleInit(NULL);
		}
		nx_ctx->rendering_mode = NX_RENDERING_MODE_CONSOLE;
	}
	const char *str = JS_ToCString(ctx, argv[0]);
	printf("%s", str);
	JS_FreeCString(ctx, str);
	return JS_UNDEFINED;
}

static JSValue js_print_err(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *str = JS_ToCString(ctx, argv[0]);
	fprintf(stderr, "%s", str);
	JS_FreeCString(ctx, str);
	return JS_UNDEFINED;
}

static JSValue js_cwd(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	char cwd[1024]; // buffer to hold current working directory

	if (getcwd(cwd, sizeof(cwd)) != NULL)
	{
		return JS_NewString(ctx, cwd);
	}
	return JS_UNDEFINED;
}

static JSValue js_chdir(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *dir = JS_ToCString(ctx, argv[0]);
	if (chdir(dir) != 0)
	{
		JS_ThrowTypeError(ctx, "%s: %s", strerror(errno), dir);
		JS_FreeCString(ctx, dir);
		return JS_EXCEPTION;
	}
	JS_FreeCString(ctx, dir);
	return JS_UNDEFINED;
}

static JSValue js_hid_initialize_touch_screen(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	hidInitializeTouchScreen();
	return JS_UNDEFINED;
}

static JSValue js_hid_initialize_keyboard(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	hidInitializeKeyboard();
	return JS_UNDEFINED;
}

static JSValue js_hid_initialize_vibration_devices(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	Result rc = hidInitializeVibrationDevices(
		nx_ctx->vibration_device_handles,
		2,
		// TODO: handle No1 gamepad
		HidNpadIdType_Handheld,
		HidNpadStyleSet_NpadStandard);
	if (R_FAILED(rc))
	{
		JS_ThrowInternalError(ctx, "hidInitializeVibrationDevices() returned 0x%x", rc);
		return JS_EXCEPTION;
	}
	return JS_UNDEFINED;
}

static JSValue js_hid_send_vibration_values(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	HidVibrationValue VibrationValues[2];
	JSValue low_amp_value = JS_GetPropertyStr(ctx, argv[0], "lowAmp");
	JSValue low_freq_value = JS_GetPropertyStr(ctx, argv[0], "lowFreq");
	JSValue high_amp_value = JS_GetPropertyStr(ctx, argv[0], "highAmp");
	JSValue high_freq_value = JS_GetPropertyStr(ctx, argv[0], "highFreq");
	double low_amp, low_freq, high_amp, high_freq;
	if (JS_ToFloat64(ctx, &low_amp, low_amp_value) ||
		JS_ToFloat64(ctx, &low_freq, low_freq_value) ||
		JS_ToFloat64(ctx, &high_amp, high_amp_value) ||
		JS_ToFloat64(ctx, &high_freq, high_freq_value))
	{
		return JS_EXCEPTION;
	}
	VibrationValues[0].freq_low = low_freq;
	VibrationValues[0].amp_low = low_amp;
	VibrationValues[0].freq_high = high_freq;
	VibrationValues[0].amp_high = high_amp;
	memcpy(&VibrationValues[1], &VibrationValues[0], sizeof(HidVibrationValue));

	Result rc = hidSendVibrationValues(nx_ctx->vibration_device_handles, VibrationValues, 2);
	if (R_FAILED(rc))
	{
		JS_ThrowInternalError(ctx, "hidSendVibrationValues() returned 0x%x", rc);
		return JS_EXCEPTION;
	}
	return JS_UNDEFINED;
}

static JSValue js_hid_get_touch_screen_states(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	HidTouchScreenState state = {0};
	hidGetTouchScreenStates(&state, 1);
	if (state.count == 0)
	{
		return JS_UNDEFINED;
	}
	JSValue arr = JS_NewArray(ctx);
	for (int i = 0; i < state.count; i++)
	{
		JSValue touch = JS_NewObject(ctx);
		JSValue x = JS_NewInt32(ctx, state.touches[i].x);
		JSValue y = JS_NewInt32(ctx, state.touches[i].y);
		JS_SetPropertyUint32(ctx, arr, i, touch);
		JS_SetPropertyStr(ctx, touch, "identifier", JS_NewInt32(ctx, state.touches[i].finger_id));
		JS_SetPropertyStr(ctx, touch, "clientX", x);
		JS_SetPropertyStr(ctx, touch, "clientY", y);
		JS_SetPropertyStr(ctx, touch, "screenX", x);
		JS_SetPropertyStr(ctx, touch, "screenY", y);
		JS_SetPropertyStr(ctx, touch, "radiusX", JS_NewFloat64(ctx, (double)state.touches[i].diameter_x / 2.0));
		JS_SetPropertyStr(ctx, touch, "radiusY", JS_NewFloat64(ctx, (double)state.touches[i].diameter_y / 2.0));
		JS_SetPropertyStr(ctx, touch, "rotationAngle", JS_NewInt32(ctx, state.touches[i].rotation_angle));
	}
	return arr;
}

static JSValue js_hid_get_keyboard_states(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	HidKeyboardState state = {0};
	hidGetKeyboardStates(&state, 1);
	JSValue obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, obj, "modifiers", JS_NewBigUint64(ctx, state.modifiers));
	for (int i = 0; i < 4; i++)
	{
		JS_SetPropertyUint32(ctx, obj, i, JS_NewBigUint64(ctx, state.keys[i]));
	}
	return obj;
}

static JSValue js_getenv(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *name = JS_ToCString(ctx, argv[0]);
	char *value = getenv(name);
	if (value == NULL)
	{
		if (errno)
		{
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

static JSValue js_setenv(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *name = JS_ToCString(ctx, argv[0]);
	const char *value = JS_ToCString(ctx, argv[1]);
	if (setenv(name, value, 1) != 0)
	{
		JS_ThrowTypeError(ctx, "%s: %s=%s", strerror(errno), name, value);
		JS_FreeCString(ctx, name);
		JS_FreeCString(ctx, value);
		return JS_EXCEPTION;
	}
	JS_FreeCString(ctx, name);
	JS_FreeCString(ctx, value);
	return JS_UNDEFINED;
}

static JSValue js_unsetenv(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	const char *name = JS_ToCString(ctx, argv[0]);
	if (unsetenv(name) != 0)
	{
		JS_ThrowTypeError(ctx, "%s: %s", strerror(errno), name);
		JS_FreeCString(ctx, name);
		return JS_EXCEPTION;
	}
	JS_FreeCString(ctx, name);
	return JS_UNDEFINED;
}

static JSValue js_env_to_object(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSValue env = JS_NewObject(ctx);

	// Get the environment variables from the operating system
	extern char **environ;
	char **envp = environ;
	while (*envp)
	{
		// Split each environment variable into a key-value pair
		char *key = strdup(*envp);
		char *eq = strchr(key, '=');
		if (eq)
		{
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
static JSValue js_get_internal_promise_state(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	JSPromiseStateEnum state = JS_PromiseState(ctx, argv[0]);
	JSValue arr = JS_NewArray(ctx);
	JS_SetPropertyUint32(ctx, arr, 0, JS_NewUint32(ctx, state));
	if (state > JS_PROMISE_PENDING)
	{
		JS_SetPropertyUint32(ctx, arr, 1, JS_PromiseResult(ctx, argv[0]));
	}
	else
	{
		JS_SetPropertyUint32(ctx, arr, 1, JS_NULL);
	}
	return arr;
}

static JSValue nx_set_frame_handler(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	nx_ctx->frame_handler = JS_DupValue(ctx, argv[0]);
	return JS_UNDEFINED;
}

static JSValue nx_set_exit_handler(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	nx_ctx->exit_handler = JS_DupValue(ctx, argv[0]);
	return JS_UNDEFINED;
}

int nx_module_set_import_meta(JSContext *ctx, JSValueConst func_val,
							  const char *url, JS_BOOL is_main)
{
	JSModuleDef *m = JS_VALUE_GET_PTR(func_val);
	JSValue meta_obj = JS_GetImportMeta(ctx, m);
	if (JS_IsException(meta_obj))
		return -1;
	JS_DefinePropertyValueStr(ctx, meta_obj, "url",
							  JS_NewString(ctx, url),
							  JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, meta_obj, "main",
							  JS_NewBool(ctx, is_main),
							  JS_PROP_C_W_E);
	JS_FreeValue(ctx, meta_obj);
	return 0;
}

void nx_process_pending_jobs(JSRuntime *rt)
{
	JSContext *ctx;
	int err;
	for (;;)
	{
		err = JS_ExecutePendingJob(rt, &ctx);
		if (err <= 0)
		{
			if (err < 0)
			{
				nx_emit_error_event(ctx);
			}
			break;
		}
	}
}

// Main program entrypoint
int main(int argc, char *argv[])
{
	Result rc;

	print_console = consoleInit(NULL);

	rc = socketInitializeDefault();
	if (R_FAILED(rc))
	{
		diagAbortWithResult(rc);
	}

	rc = romfsInit();
	if (R_FAILED(rc))
	{
		diagAbortWithResult(rc);
	}

	rc = plInitialize(PlServiceType_User);
	if (R_FAILED(rc))
	{
		diagAbortWithResult(rc);
	}

	FILE *debug_fd = freopen(LOG_FILENAME, "w", stderr);

	// Configure our supported input layout: a single player with standard controller styles
	padConfigureInput(1, HidNpadStyleSet_NpadStandard);

	// Initialize the default gamepad (which reads handheld mode inputs as well as the first connected controller)
	PadState pad;
	padInitializeDefault(&pad);

	JSRuntime *rt = JS_NewRuntime();
	JSContext *ctx = JS_NewContext(rt);

	nx_context_t *nx_ctx = malloc(sizeof(nx_context_t));
	memset(nx_ctx, 0, sizeof(nx_context_t));
	nx_ctx->rendering_mode = NX_RENDERING_MODE_CONSOLE;
	nx_ctx->thpool = thpool_init(4);
	nx_ctx->frame_handler = JS_UNDEFINED;
	nx_ctx->exit_handler = JS_UNDEFINED;
	nx_ctx->error_handler = JS_UNDEFINED;
	nx_ctx->unhandled_rejection_handler = JS_UNDEFINED;
	pthread_mutex_init(&(nx_ctx->async_done_mutex), NULL);
	JS_SetContextOpaque(ctx, nx_ctx);
	JS_SetHostPromiseRejectionTracker(rt, nx_promise_rejection_handler, ctx);

	// First try the `main.js` file on the RomFS
	size_t user_code_size;
	int js_path_needs_free = 0;
	char *js_path = "romfs:/main.js";
	char *user_code = (char *)read_file(js_path, &user_code_size);
	if (user_code == NULL && errno == ENOENT)
	{
		// If no `main.js`, then try the `.js file with the
		// matching name as the `.nro` file on the SD card
		js_path_needs_free = 1;
		js_path = strdup(argv[0]);
		size_t js_path_len = strlen(js_path);
		char *dot_nro = strstr(js_path, ".nro");
		if (dot_nro != NULL && (dot_nro - js_path) == js_path_len - 4)
		{
			strcpy(dot_nro, ".js");
		}

		user_code = (char *)read_file(js_path, &user_code_size);
	}
	if (user_code == NULL)
	{
		printf("%s: %s\n", strerror(errno), js_path);
		if (js_path_needs_free)
		{
			free(js_path);
		}
		nx_ctx->had_error = 1;
		goto main_loop;
	}

	// The internal `$` object contains native functions that are wrapped in the JS runtime
	JSValue global_obj = JS_GetGlobalObject(ctx);
	JSValue init_obj = JS_NewObject(ctx);
	nx_init_account(ctx, init_obj);
	nx_init_applet(ctx, init_obj);
	nx_init_battery(ctx, init_obj);
	nx_init_canvas(ctx, init_obj);
	nx_init_crypto(ctx, init_obj);
	nx_init_dns(ctx, init_obj);
	nx_init_error(ctx, init_obj);
	nx_init_font(ctx, init_obj);
	nx_init_fs(ctx, init_obj);
	nx_init_fsdev(ctx, init_obj);
	nx_init_image(ctx, init_obj);
	nx_init_irs(ctx, init_obj);
	nx_init_nifm(ctx, init_obj);
	nx_init_ns(ctx, init_obj);
	nx_init_tcp(ctx, init_obj);
	nx_init_tls(ctx, init_obj);
	nx_init_swkbd(ctx, init_obj);
	nx_init_wasm(ctx, init_obj);
	const JSCFunctionListEntry init_function_list[] = {
		JS_CFUNC_DEF("exit", 0, js_exit),
		JS_CFUNC_DEF("cwd", 0, js_cwd),
		JS_CFUNC_DEF("chdir", 1, js_chdir),
		JS_CFUNC_DEF("print", 1, js_print),
		JS_CFUNC_DEF("printErr", 1, js_print_err),
		JS_CFUNC_DEF("getInternalPromiseState", 1, js_get_internal_promise_state),
		JS_CFUNC_DEF("hidInitializeTouchScreen", 0, js_hid_initialize_touch_screen),
		JS_CFUNC_DEF("hidGetTouchScreenStates", 0, js_hid_get_touch_screen_states),

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
		JS_CFUNC_DEF("hidInitializeVibrationDevices", 0, js_hid_initialize_vibration_devices),
		JS_CFUNC_DEF("hidGetKeyboardStates", 0, js_hid_get_keyboard_states),
		JS_CFUNC_DEF("hidSendVibrationValues", 0, js_hid_send_vibration_values),
	};
	JS_SetPropertyFunctionList(ctx, init_obj, init_function_list, countof(init_function_list));

	// `Switch.version`
	JSValue version_obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, version_obj, "cairo", JS_NewString(ctx, cairo_version_string()));
	JS_SetPropertyStr(ctx, version_obj, "freetype2", JS_NewString(ctx, FREETYPE_VERSION_STR));
	JS_SetPropertyStr(ctx, version_obj, "harfbuzz", JS_NewString(ctx, HB_VERSION_STRING));
	JS_SetPropertyStr(ctx, version_obj, "mbedtls", JS_NewString(ctx, MBEDTLS_VERSION_STRING));
	JS_SetPropertyStr(ctx, version_obj, "nxjs", JS_NewString(ctx, NXJS_VERSION));
	JS_SetPropertyStr(ctx, version_obj, "png", JS_NewString(ctx, PNG_LIBPNG_VER_STRING));
	JS_SetPropertyStr(ctx, version_obj, "quickjs", JS_NewString(ctx, JS_GetVersion()));
	JS_SetPropertyStr(ctx, version_obj, "turbojpeg", JS_NewString(ctx, LIBTURBOJPEG_VERSION));
	JS_SetPropertyStr(ctx, version_obj, "wasm3", JS_NewString(ctx, M3_VERSION));
	const int webp_version = WebPGetDecoderVersion();
	char webp_version_str[12];
	snprintf(webp_version_str, 12, "%d.%d.%d", (webp_version >> 16) & 0xFF, (webp_version >> 8) & 0xFF, webp_version & 0xFF);
	JS_SetPropertyStr(ctx, version_obj, "webp", JS_NewString(ctx, webp_version_str));
	JS_SetPropertyStr(ctx, init_obj, "version", version_obj);

	// `Switch.entrypoint`
	JS_SetPropertyStr(ctx, init_obj, "entrypoint", JS_NewString(ctx, js_path));

	// `Switch.argv`
	JSValue argv_array = JS_NewArray(ctx);
	for (int i = 0; i < argc; i++)
	{
		JS_SetPropertyUint32(ctx, argv_array, i, JS_NewString(ctx, argv[i]));
	}
	JS_SetPropertyStr(ctx, init_obj, "argv", argv_array);

	JS_SetPropertyStr(ctx, global_obj, "$", init_obj);

	size_t runtime_buffer_size;
	char *runtime_path = "romfs:/runtime.js";
	char *runtime_buffer = (char *)read_file(runtime_path, &runtime_buffer_size);
	if (runtime_buffer == NULL)
	{
		printf("%s: %s\n", strerror(errno), runtime_path);
		nx_ctx->had_error = 1;
		goto main_loop;
	}

	JSValue runtime_init_result = JS_Eval(ctx, runtime_buffer, runtime_buffer_size, runtime_path, JS_EVAL_TYPE_GLOBAL);
	if (JS_IsException(runtime_init_result))
	{
		print_js_error(ctx);
		nx_ctx->had_error = 1;
	}
	JS_FreeValue(ctx, runtime_init_result);
	free(runtime_buffer);
	if (nx_ctx->had_error)
	{
		goto main_loop;
	}

	// Run the user code
	JSValue user_code_result = JS_Eval(ctx, user_code, user_code_size, js_path, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
	if (JS_IsException(user_code_result))
	{
		nx_emit_error_event(ctx);
	}
	else
	{
		nx_module_set_import_meta(ctx, user_code_result, js_path, true);
		user_code_result = JS_EvalFunction(ctx, user_code_result);
		if (JS_IsException(user_code_result))
		{
			nx_emit_error_event(ctx);
		}
	}
	JS_FreeValue(ctx, user_code_result);
	free(user_code);
	if (js_path_needs_free)
	{
		free(js_path);
	}

main_loop:
	while (appletMainLoop())
	{
		if (!nx_ctx->had_error)
		{
			// Check if any file descriptors have reported activity
			nx_poll(&nx_ctx->poll);
		}

		// Check if any thread pool tasks have completed
		if (!nx_ctx->had_error)
			nx_process_async(ctx, nx_ctx);

		// Process any Promises that need to be fulfilled
		if (!nx_ctx->had_error)
			nx_process_pending_jobs(rt);

		padUpdate(&pad);
		u64 kDown = padGetButtons(&pad);

		if (nx_ctx->had_error)
		{
			if (kDown & HidNpadButton_Plus)
			{
				// When an initialization or unhandled error occurs,
				// wait until the user presses "+" to fully exit so
				// the user has a chance to read the error message.
				break;
			}
		}
		else
		{
			// Call frame handler
			JSValueConst args[] = {JS_NewUint32(ctx, kDown)};
			JSValue ret_val = JS_Call(ctx, nx_ctx->frame_handler, JS_NULL, 1, args);

			if (JS_IsException(ret_val))
			{
				nx_emit_error_event(ctx);
			}
			JS_FreeValue(ctx, ret_val);

			if (!is_running)
			{
				// `Switch.exit()` was called
				break;
			}
		}

		if (nx_ctx->rendering_mode == NX_RENDERING_MODE_CONSOLE)
		{
			// Update the console, sending a new frame to the display
			consoleUpdate(print_console);
		}
		else if (nx_ctx->rendering_mode == NX_RENDERING_MODE_CANVAS)
		{
			// Copy the JS framebuffer to the current Switch buffer
			u32 stride;
			u8 *framebuf = (u8 *)framebufferBegin(framebuffer, &stride);
			memcpy(framebuf, js_framebuffer, 1280 * 720 * 4);
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

	if (nx_ctx->rendering_mode == NX_RENDERING_MODE_CONSOLE)
	{
		nx_console_exit();
	}
	else if (nx_ctx->rendering_mode == NX_RENDERING_MODE_CANVAS)
	{
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

	if (nx_ctx->wasm_env)
	{
		m3_FreeEnvironment(nx_ctx->wasm_env);
	}
	if (nx_ctx->ft_library)
	{
		FT_Done_FreeType(nx_ctx->ft_library);
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
