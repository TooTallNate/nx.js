#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <inttypes.h>
#include <switch.h>
#include <quickjs.h>

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
    int width = 1280;
    int height = 720;
    framebufferCreate(framebuffer, win, width, height, PIXEL_FORMAT_RGBA_8888, 2);
    framebufferMakeLinear(framebuffer);
    size_t buf_size; /* should result in `width * height * 4` */
    js_framebuffer = JS_GetArrayBuffer(ctx, &buf_size, argv[0]);
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

char *read_file(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
    {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char *buffer = malloc(size + 1);
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

    buffer[size] = '\0';
    return buffer;
}

void print_js_error(JSContext *ctx)
{
    JSValue exception_val = JS_GetException(ctx);
    const char *exception_str = JS_ToCString(ctx, exception_val);
    printf("%s\n", exception_str);
    JS_FreeCString(ctx, exception_str);

    JSValue stack_val = JS_GetPropertyStr(ctx, exception_val, "stack");
    const char *stack_str = JS_ToCString(ctx, stack_val);
    printf("%s\n", stack_str);
    JS_FreeCString(ctx, stack_str);

    JS_FreeValue(ctx, exception_val);
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

static JSValue js_readdir_sync(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    DIR *dir;
    struct dirent *entry;

    const char *path = JS_ToCString(ctx, argv[0]);
    dir = opendir(path);
    if (dir == NULL)
    {
        JSValue error = JS_NewString(ctx, "An error occurred");
        JS_Throw(ctx, error);
        return JS_UNDEFINED;
    }

    int i = 0;
    JSValue arr = JS_NewArray(ctx);
    while ((entry = readdir(dir)) != NULL)
    {
        JS_SetPropertyUint32(ctx, arr, i, JS_NewString(ctx, entry->d_name));
        i++;
    }

    closedir(dir);

    return arr;
}

static JSValue js_getenv(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *name = JS_ToCString(ctx, argv[0]);
    char *value = getenv(name);
    JS_FreeCString(ctx, name);
    return JS_NewString(ctx, value);
}

static JSValue js_setenv(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *name = JS_ToCString(ctx, argv[0]);
    const char *value = JS_ToCString(ctx, argv[1]);
    setenv(name, value, 1);
    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, value);
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
        envp++;
    }

    return env;
}

static JSValue js_appletGetOperationMode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    return JS_NewInt32(ctx, appletGetOperationMode());
}

// Main program entrypoint
int main(int argc, char *argv[])
{
    // Configure our supported input layout: a single player with standard controller styles
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    // Initialize the default gamepad (which reads handheld mode inputs as well as the first connected controller)
    PadState pad;
    padInitializeDefault(&pad);

    // Replace `.nro` with `.js`
    char *js_path = strdup(argv[0]);
    size_t js_path_len = strlen(js_path);
    char *dot_nro = strstr(js_path, ".nro");
    if (dot_nro != NULL && (dot_nro - js_path) == js_path_len - 4)
    {
        strcpy(dot_nro, ".js");
    }

    char *buffer = read_file(js_path);
    if (buffer == NULL)
    {
        printf("Failed to load JS file: %s\n", js_path);
    }

    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);

    // Initialize the JS runtime environment
    Result rc = romfsInit();
    if (R_FAILED(rc))
    {
        printf("romfsInit failed: %08X\n", rc);
    }
    else
    {
        char *runtime_path = "romfs:/runtime.js";
        char *runtime_buffer = read_file(runtime_path);
        if (runtime_buffer == NULL)
        {
            printf("Failed to initialize JS runtime\n");
        }
        else
        {
            JSValue runtime_init_result = JS_Eval(ctx, runtime_buffer, strlen(runtime_buffer), runtime_path, JS_EVAL_TYPE_GLOBAL);
            if (JS_IsException(runtime_init_result))
            {
                print_js_error(ctx);
            }
            JS_FreeValue(ctx, runtime_init_result);
            free(runtime_buffer);
        }
    }

    JSValue global_obj = JS_GetGlobalObject(ctx);
    JSValue switch_obj = JS_GetPropertyStr(ctx, global_obj, "Switch");
    JSValue native_obj = JS_GetPropertyStr(ctx, switch_obj, "native");
    JSValue switch_dispatch_func = JS_GetPropertyStr(ctx, switch_obj, "dispatchEvent");

    JSValue exit_func = JS_NewCFunction(ctx, js_exit, "exit", 0);
    JS_SetPropertyStr(ctx, switch_obj, "exit", exit_func);

    const JSCFunctionListEntry function_list[] = {
        JS_CFUNC_DEF("print", 1, js_print),
        JS_CFUNC_DEF("cwd", 0, js_cwd),

        // env vars
        JS_CFUNC_DEF("getenv", 1, js_getenv),
        JS_CFUNC_DEF("setenv", 2, js_setenv),
        JS_CFUNC_DEF("envToObject", 0, js_env_to_object),

        // console renderer
        JS_CFUNC_DEF("consoleInit", 0, js_console_init),
        JS_CFUNC_DEF("consoleExit", 0, js_console_exit),

        // framebuffer renderer
        JS_CFUNC_DEF("framebufferInit", 0, js_framebuffer_init),
        JS_CFUNC_DEF("framebufferExit", 0, js_framebuffer_exit),

        // filesystem
        JS_CFUNC_DEF("readDirSync", 0, js_readdir_sync),

        // applet
        JS_CFUNC_DEF("appletGetOperationMode", 0, js_appletGetOperationMode),
    };
    JS_SetPropertyFunctionList(ctx, native_obj, function_list, 11);

    // `Switch.argv`
    JSValue argv_array = JS_NewArray(ctx);
    for (int i = 0; i < argc; i++)
    {
        JS_SetPropertyUint32(ctx, argv_array, 0, JS_NewString(ctx, argv[i]));
    }
    JS_SetPropertyStr(ctx, switch_obj, "argv", argv_array);

    // Run the user code
    if (buffer != NULL)
    {
        JSValue user_code_result = JS_Eval(ctx, buffer, strlen(buffer), js_path, JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(user_code_result))
        {
            print_js_error(ctx);
        }
        JS_FreeValue(ctx, user_code_result);
        free(buffer);
        free(js_path);
    }

    // Main loop
    while (appletMainLoop())
    {
        // Scan the gamepad. This should be done once for each frame
        padUpdate(&pad);

        // padGetButtonsDown returns the set of buttons that have been
        // newly pressed in this frame compared to the previous one
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_Plus)
        {
            is_running = 0;
        }

        // Dispatch "frame" event
        JSValue event_obj = JS_NewObject(ctx);
        JSValue event_type = JS_NewString(ctx, "frame");
        JS_SetPropertyStr(ctx, event_obj, "type", event_type);
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
            // Retrieve the framebuffer
            u32 stride;
            u8 *framebuf = (u8 *)framebufferBegin(framebuffer, &stride);

            memcpy(framebuf, js_framebuffer, 1280 * 720 * 4);
            //u32 *f = malloc(1280 * 720 * 4);
            //memset(f, 0, 1280 * 720 * 4);
            //u32 yy = 100;
            //for (u32 x = 0; x < 1280; x++)
            //{
            //    u32 pos = (yy * 1280) + x;
            //    f[pos] = 0x01010101 * 10 * 4; // Set framebuf to different shades of grey.
            //}

            //// memcpy
            //for (u32 y = 0; y < 720; y++)
            //{
            //    for (u32 x = 0; x < 1280; x++)
            //    {
            //        u32 pos = y * stride / sizeof(u32) + x;
            //        framebuf[pos] = f[(y * 1280) + x];
            //        //framebuf[pos] = 0x01010101 * 50 * 4; // Set framebuf to different shades of grey.
            //    }
            //}

            //free(f);

            framebufferEnd(framebuffer);
        }
        else if (print_console != NULL)
        {
            // Update the console, sending a new frame to the display
            consoleUpdate(print_console);
        }
    }

    // Dispatch "exit" event
    JSValue event_obj = JS_NewObject(ctx);
    JSValue event_type = JS_NewString(ctx, "exit");
    JS_SetPropertyStr(ctx, event_obj, "type", event_type);
    JSValue args[] = {event_obj};
    JSValue ret_val = JS_Call(ctx, switch_dispatch_func, switch_obj, 1, args);
    JS_FreeValue(ctx, event_obj);
    if (JS_IsException(ret_val))
    {
        print_js_error(ctx);
    }
    JS_FreeValue(ctx, ret_val);

    // JS_FreeValue(ctx, argv_array);
    // JS_FreeValue(ctx, switch_dispatch_func);
    // JS_FreeValue(ctx, native_obj);
    // JS_FreeValue(ctx, switch_obj);
    // JS_FreeValue(ctx, global_obj);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    return 0;
}
