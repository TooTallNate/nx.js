#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <switch.h>
#include <quickjs/quickjs.h>

#include "types.h"
#include "async.h"
#include "applet.h"
#include "crypto.h"
#include "dns.h"
#include "error.h"
#include "canvas.h"
#include "font.h"
#include "fs.h"
#include "image.h"
#include "tcp.h"
#include "poll.h"

// Text renderer
static PrintConsole *print_console = NULL;

// Framebuffer renderer
static NWindow *win = NULL;
static Framebuffer *framebuffer = NULL;
static uint8_t *js_framebuffer = NULL;

static JSValue js_console_init(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    if (print_console == NULL)
    {
        print_console = consoleInit(NULL);
    }
    return JS_UNDEFINED;
}

static JSValue js_console_exit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    if (print_console != NULL)
    {
        consoleExit(NULL);
        print_console = NULL;
    }
    return JS_UNDEFINED;
}

static JSValue js_framebuffer_init(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
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
    framebuffer = malloc(sizeof(Framebuffer));
    nx_canvas_context_2d_t *context = nx_get_canvas_context_2d(ctx, argv[0]);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    framebufferCreate(framebuffer, win, context->width, context->height, PIXEL_FORMAT_BGRA_8888, 2);
    framebufferMakeLinear(framebuffer);
    js_framebuffer = context->data;
    return JS_UNDEFINED;
}

static JSValue js_framebuffer_exit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    if (framebuffer != NULL)
    {
        framebufferClose(framebuffer);
        free(framebuffer);
        framebuffer = NULL;
        js_framebuffer = NULL;
    }
    return JS_UNDEFINED;
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

static int is_running = 1;

static JSValue js_exit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    is_running = 0;
    return JS_UNDEFINED;
}

static JSValue js_print(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *str = JS_ToCString(ctx, argv[0]);
    printf("%s", str);
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
    JSPromiseData *promise_data = JS_GetOpaque(argv[0], JS_CLASS_PROMISE);
    JSValue arr = JS_NewArray(ctx);
    JS_SetPropertyUint32(ctx, arr, 0, JS_NewInt32(ctx, promise_data->promise_state));
    if (promise_data->promise_state > 0)
    {
        JS_SetPropertyUint32(ctx, arr, 1, promise_data->promise_result);
    }
    else
    {
        JS_SetPropertyUint32(ctx, arr, 1, JS_NULL);
    }
    return arr;
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
                print_js_error(ctx);
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

    // Configure our supported input layout: a single player with standard controller styles
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    // Initialize the default gamepad (which reads handheld mode inputs as well as the first connected controller)
    PadState pad;
    padInitializeDefault(&pad);

    int had_error = 0;

    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);

    nx_context_t *nx_ctx = malloc(sizeof(nx_context_t));
    memset(nx_ctx, 0, sizeof(nx_context_t));
    nx_ctx->thpool = thpool_init(4);
    pthread_mutex_init(&(nx_ctx->async_done_mutex), NULL);
    JS_SetContextOpaque(ctx, nx_ctx);

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
        had_error = 1;
        goto wait_error;
    }

    size_t runtime_buffer_size;
    char *runtime_path = "romfs:/runtime.js";
    char *runtime_buffer = (char *)read_file(runtime_path, &runtime_buffer_size);
    if (runtime_buffer == NULL)
    {
        printf("%s: %s\n", strerror(errno), runtime_path);
        had_error = 1;
        goto wait_error;
    }

    JSValue runtime_init_result = JS_Eval(ctx, runtime_buffer, runtime_buffer_size, runtime_path, JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(runtime_init_result))
    {
        print_js_error(ctx);
        had_error = 1;
    }
    JS_FreeValue(ctx, runtime_init_result);
    free(runtime_buffer);
    if (had_error)
    {
        goto wait_error;
    }

    JSValue global_obj = JS_GetGlobalObject(ctx);
    JSValue switch_obj = JS_GetPropertyStr(ctx, global_obj, "Switch");
    JSValue native_obj = JS_GetPropertyStr(ctx, switch_obj, "native");
    JSValue switch_dispatch_func = JS_GetPropertyStr(ctx, switch_obj, "dispatchEvent");

    JSValue version_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, version_obj, "nxjs", JS_NewString(ctx, NXJS_VERSION));
    JS_SetPropertyStr(ctx, version_obj, "cairo", JS_NewString(ctx, cairo_version_string()));
    JS_SetPropertyStr(ctx, version_obj, "freetype2", JS_NewString(ctx, FREETYPE_VERSION_STR));
    JS_SetPropertyStr(ctx, version_obj, "quickjs", JS_NewString(ctx, QUICKJS_VERSION));
    JS_SetPropertyStr(ctx, switch_obj, "version", version_obj);

    nx_init_applet(ctx, native_obj);
    nx_init_crypto(ctx, native_obj);
    nx_init_canvas(ctx, native_obj);
    nx_init_font(ctx, native_obj);
    nx_init_fs(ctx, native_obj);
    nx_init_image(ctx, native_obj);
    nx_init_tcp(ctx, native_obj);

    JSValue exit_func = JS_NewCFunction(ctx, js_exit, "exit", 0);
    JS_SetPropertyStr(ctx, switch_obj, "exit", exit_func);

    const JSCFunctionListEntry function_list[] = {
        JS_CFUNC_DEF("print", 1, js_print),
        JS_CFUNC_DEF("cwd", 0, js_cwd),
        JS_CFUNC_DEF("chdir", 1, js_chdir),
        JS_CFUNC_DEF("getInternalPromiseState", 0, js_get_internal_promise_state),

        // env vars
        JS_CFUNC_DEF("getenv", 1, js_getenv),
        JS_CFUNC_DEF("setenv", 2, js_setenv),
        JS_CFUNC_DEF("unsetenv", 1, js_unsetenv),
        JS_CFUNC_DEF("envToObject", 0, js_env_to_object),

        // hid
        JS_CFUNC_DEF("hidInitializeKeyboard", 0, js_hid_initialize_keyboard),
        JS_CFUNC_DEF("hidInitializeTouchScreen", 0, js_hid_initialize_touch_screen),
        JS_CFUNC_DEF("hidGetKeyboardStates", 0, js_hid_get_keyboard_states),
        JS_CFUNC_DEF("hidGetTouchScreenStates", 0, js_hid_get_touch_screen_states),

        // console renderer
        JS_CFUNC_DEF("consoleInit", 0, js_console_init),
        JS_CFUNC_DEF("consoleExit", 0, js_console_exit),

        // framebuffer renderer
        JS_CFUNC_DEF("framebufferInit", 0, js_framebuffer_init),
        JS_CFUNC_DEF("framebufferExit", 0, js_framebuffer_exit),

        // dns
        JS_CFUNC_DEF("resolveDns", 0, js_dns_resolve)};
    JS_SetPropertyFunctionList(ctx, native_obj, function_list, countof(function_list));

    // `Switch.entrypoint`
    JS_SetPropertyStr(ctx, switch_obj, "entrypoint", JS_NewString(ctx, js_path));

    // `Switch.argv`
    JSValue argv_array = JS_NewArray(ctx);
    for (int i = 0; i < argc; i++)
    {
        JS_SetPropertyUint32(ctx, argv_array, i, JS_NewString(ctx, argv[i]));
    }
    JS_SetPropertyStr(ctx, switch_obj, "argv", argv_array);

    // Run the user code
    JSValue user_code_result = JS_Eval(ctx, user_code, user_code_size, js_path, JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(user_code_result))
    {
        print_js_error(ctx);
        had_error = 1;
    }
    JS_FreeValue(ctx, user_code_result);
    free(user_code);
    if (js_path_needs_free)
    {
        free(js_path);
    }
    if (had_error)
    {
        goto wait_error;
    }

    // Main loop
    while (appletMainLoop())
    {
        // Check if any file descriptors have reported activity
        nx_poll(&nx_ctx->poll);

        // Check if any thread pool tasks have completed
        nx_process_async(ctx, nx_ctx);

        // Process any Promises that need to be fulfilled
        nx_process_pending_jobs(rt);

        padUpdate(&pad);
        u64 kDown = padGetButtons(&pad);

        // Dispatch "frame" event
        JSValue event_obj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, event_obj, "type", JS_NewString(ctx, "frame"));
        JS_SetPropertyStr(ctx, event_obj, "detail", JS_NewUint32(ctx, kDown));
        JSValue args[] = {event_obj};
        JSValue ret_val = JS_Call(ctx, switch_dispatch_func, switch_obj, 1, args);
        JS_FreeValue(ctx, event_obj);

        if (!is_running)
        {
            // `Switch.exit()` was called
            JS_FreeValue(ctx, ret_val);
            break;
        }

        if (JS_IsException(ret_val))
        {
            print_js_error(ctx);
        }
        JS_FreeValue(ctx, ret_val);

        if (framebuffer != NULL)
        {
            // Copy the JS framebuffer to the current Switch buffer
            u32 stride;
            u8 *framebuf = (u8 *)framebufferBegin(framebuffer, &stride);
            memcpy(framebuf, js_framebuffer, 1280 * 720 * 4);
            framebufferEnd(framebuffer);
        }
        else if (print_console != NULL)
        {
            // Update the console, sending a new frame to the display
            consoleUpdate(print_console);
        }
    }

    // XXX: Ideally we wouldn't `thpool_wait()` here,
    // but the app seems to crash without it
    thpool_wait(nx_ctx->thpool);
    thpool_destroy(nx_ctx->thpool);

    // Dispatch "exit" event
    JSValue event_obj = JS_NewObject(ctx);
    JSValue event_type = JS_NewString(ctx, "exit");
    JS_SetPropertyStr(ctx, event_obj, "type", event_type);
    JSValue args[] = {event_obj};
    JSValue ret_val = JS_Call(ctx, switch_dispatch_func, switch_obj, 1, args);
    JS_FreeValue(ctx, event_obj);
    JS_FreeValue(ctx, ret_val);

wait_error:
    if (had_error)
    {
        // When an initialization or unhandled error occurs,
        // wait until the user presses "+" to fully exit so
        // the user has a chance to read the error message.
        while (appletMainLoop())
        {
            padUpdate(&pad);
            u64 kDown = padGetButtonsDown(&pad);
            if (kDown & HidNpadButton_Plus) break;
            consoleUpdate(NULL);
        }
    }

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    if (nx_ctx->ft_library)
    {
        FT_Done_FreeType(nx_ctx->ft_library);
    }

    free(nx_ctx);

    return 0;
}
