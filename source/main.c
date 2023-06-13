#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <switch.h>
#include <quickjs/quickjs.h>
#include <cairo.h>
#include <cairo-ft.h>
#include <ft2build.h>

#include "types.h"
#include "applet.h"
#include "font.h"
#include "fs.h"

// Text renderer
static PrintConsole *print_console = NULL;

// Framebuffer renderer
static NWindow *win = NULL;
static Framebuffer *framebuffer = NULL;
static uint8_t *js_framebuffer = NULL;

static JSClassID js_canvas_context_class_id;

typedef struct
{
    uint32_t width;
    uint32_t height;
    uint8_t *data;
    cairo_t *ctx;
    cairo_surface_t *surface;
    cairo_path_t *path;
    // cairo_font_face_t *font;
    FT_Face ft_face;
} CanvasContext2D;

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
    CanvasContext2D *context = JS_GetOpaque2(ctx, argv[0], js_canvas_context_class_id);
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

    buffer[size + 1] = '\0';

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
        JS_SetPropertyUint32(ctx, arr, i, touch);
        JS_SetPropertyStr(ctx, touch, "identifier", JS_NewInt32(ctx, state.touches[i].finger_id));
        JS_SetPropertyStr(ctx, touch, "screenX", JS_NewInt32(ctx, state.touches[i].x));
        JS_SetPropertyStr(ctx, touch, "screenY", JS_NewInt32(ctx, state.touches[i].y));
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

static JSValue js_canvas_new_context(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int width;
    int height;
    if (JS_ToInt32(ctx, &width, argv[0]) ||
        JS_ToInt32(ctx, &height, argv[1]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    size_t buf_size = width * height * 4;
    uint8_t *buffer = js_malloc(ctx, buf_size);
    if (!buffer)
    {
        JS_ThrowOutOfMemory(ctx);
        return JS_EXCEPTION;
    }
    memset(buffer, 0, buf_size);

    CanvasContext2D *context = js_malloc(ctx, sizeof(CanvasContext2D));
    if (!context)
    {
        JS_ThrowOutOfMemory(ctx);
        return JS_EXCEPTION;
    }
    memset(context, 0, sizeof(CanvasContext2D));

    // printf("1: %p\n", (void*)context);
    JSValue obj = JS_NewObjectClass(ctx, js_canvas_context_class_id);
    if (JS_IsException(obj))
    {
        free(context);
        return obj;
    }

    // TOOD: this probably needs to go into `framebuffer_init` instead
    JS_DupValue(ctx, obj);

    // On Switch, the byte order seems to be BGRA
    cairo_surface_t *surface = cairo_image_surface_create_for_data(
        buffer, CAIRO_FORMAT_ARGB32, width, height, width * 4);

    context->width = width;
    context->height = height;
    context->data = buffer;
    context->surface = surface;
    context->ctx = cairo_create(surface);

    cairo_set_font_size(context->ctx, 46);

    JS_SetOpaque(obj, context);
    return obj;
}

static JSValue js_canvas_set_fill_style(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    CanvasContext2D *context = JS_GetOpaque2(ctx, argv[0], js_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    double r;
    double g;
    double b;
    double a;
    if (JS_ToFloat64(ctx, &r, argv[1]) ||
        JS_ToFloat64(ctx, &g, argv[2]) ||
        JS_ToFloat64(ctx, &b, argv[3]) ||
        JS_ToFloat64(ctx, &a, argv[4]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    cairo_set_source_rgba(context->ctx, r / (double)255, g / (double)255, b / (double)255, a);
    return JS_UNDEFINED;
}

static JSValue js_canvas_set_font(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    CanvasContext2D *context = JS_GetOpaque2(ctx, argv[0], js_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    nx_font_face_t *face = nx_get_font_face(ctx, argv[1]);
    if (!face)
    {
        return JS_EXCEPTION;
    }
    double font_size;
    if (JS_ToFloat64(ctx, &font_size, argv[2]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    cairo_set_font_face(context->ctx, face->cairo_font);
    cairo_set_font_size(context->ctx, font_size);
    context->ft_face = face->ft_face;
    return JS_UNDEFINED;
}

static JSValue js_canvas_set_line_width(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    CanvasContext2D *context = JS_GetOpaque2(ctx, argv[0], js_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    double n;
    if (JS_ToFloat64(ctx, &n, argv[1]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    cairo_set_line_width(context->ctx, n);
    return JS_UNDEFINED;
}

static JSValue js_canvas_fill_rect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    CanvasContext2D *context = JS_GetOpaque2(ctx, argv[0], js_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    double x;
    double y;
    double w;
    double h;
    if (JS_ToFloat64(ctx, &x, argv[1]) ||
        JS_ToFloat64(ctx, &y, argv[2]) ||
        JS_ToFloat64(ctx, &w, argv[3]) ||
        JS_ToFloat64(ctx, &h, argv[4]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    cairo_rectangle(
        context->ctx,
        x, y,
        w,
        h);
    cairo_fill(context->ctx);
    return JS_UNDEFINED;
}

static JSValue js_canvas_fill_text(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    CanvasContext2D *context = JS_GetOpaque2(ctx, argv[0], js_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    double x;
    double y;
    if (JS_ToFloat64(ctx, &x, argv[2]) ||
        JS_ToFloat64(ctx, &y, argv[3]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    const char *text = JS_ToCString(ctx, argv[1]);
    cairo_move_to(context->ctx, x, y);
    cairo_show_text(context->ctx, text);
    JS_FreeCString(ctx, text);
    return JS_UNDEFINED;
}

static JSValue js_canvas_measure_text(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    CanvasContext2D *context = JS_GetOpaque2(ctx, argv[0], js_canvas_context_class_id);
    if (!context)
    {
        return JS_EXCEPTION;
    }
    const char *text = JS_ToCString(ctx, argv[1]);
    cairo_text_extents_t extents;
    cairo_text_extents(context->ctx, text, &extents);
    JS_FreeCString(ctx, text);
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "xBearing", JS_NewFloat64(ctx, extents.x_bearing));
    JS_SetPropertyStr(ctx, obj, "yBearing", JS_NewFloat64(ctx, extents.y_bearing));
    JS_SetPropertyStr(ctx, obj, "xAdvance", JS_NewFloat64(ctx, extents.x_advance));
    JS_SetPropertyStr(ctx, obj, "yAdvance", JS_NewFloat64(ctx, extents.y_advance));
    JS_SetPropertyStr(ctx, obj, "width", JS_NewFloat64(ctx, extents.width));
    JS_SetPropertyStr(ctx, obj, "height", JS_NewFloat64(ctx, extents.height));
    return obj;
}

static JSValue js_canvas_get_image_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int sx;
    int sy;
    int sw;
    int sh;
    int cw;
    size_t length;
    uint32_t *buffer = (uint32_t *)JS_GetArrayBuffer(ctx, &length, argv[0]);
    if (JS_ToInt32(ctx, &sx, argv[1]) ||
        JS_ToInt32(ctx, &sy, argv[2]) ||
        JS_ToInt32(ctx, &sw, argv[3]) ||
        JS_ToInt32(ctx, &sh, argv[4]) ||
        JS_ToInt32(ctx, &cw, argv[5]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }

    // Create a new ArrayBuffer with managed data
    size_t size = sw * sh * 4;
    uint32_t *new_buffer = js_malloc(ctx, size);
    if (!new_buffer)
    {
        JS_ThrowOutOfMemory(ctx);
        return JS_EXCEPTION;
    }

    // Fill the buffer with some data
    memset(new_buffer, 0, size);
    for (int y = 0; y < sh; y++)
    {
        for (int x = 0; x < sw; x++)
        {
            new_buffer[(y * sw) + x] = buffer[(y * cw) + x];
        }
    }

    // Create the ArrayBuffer object
    return JS_NewArrayBuffer(ctx, (uint8_t *)new_buffer, size, NULL, NULL, 0);
}

static JSValue js_canvas_put_image_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int dx;
    int dy;
    int dirty_x;
    int dirty_y;
    int dirty_width;
    int dirty_height;
    int cw;
    size_t source_length;
    size_t dest_length;
    uint32_t *source_buffer = (uint32_t *)JS_GetArrayBuffer(ctx, &source_length, argv[0]);
    uint32_t *dest_buffer = (uint32_t *)JS_GetArrayBuffer(ctx, &dest_length, argv[1]);
    if (JS_ToInt32(ctx, &dx, argv[2]) ||
        JS_ToInt32(ctx, &dy, argv[3]) ||
        JS_ToInt32(ctx, &dirty_x, argv[4]) ||
        JS_ToInt32(ctx, &dirty_y, argv[5]) ||
        JS_ToInt32(ctx, &dirty_width, argv[6]) ||
        JS_ToInt32(ctx, &dirty_height, argv[7]) ||
        JS_ToInt32(ctx, &cw, argv[8]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }
    int dest_x;
    int dest_y;
    for (int y = dirty_y; y < dirty_height; y++)
    {
        for (int x = dirty_x; x < dirty_width; x++)
        {
            dest_x = dx + x;
            dest_y = dy + y;
            dest_buffer[(dest_y * cw) + dest_x] = source_buffer[(y * dirty_width) + x];
        }
    }
    return JS_UNDEFINED;
}

static void finalizer_canvas_context_2d(JSRuntime *rt, JSValue val)
{
    CanvasContext2D *context = JS_GetOpaque(val, js_canvas_context_class_id);
    if (context)
    {
        cairo_destroy(context->ctx);
        cairo_surface_destroy(context->surface);
        js_free_rt(rt, context->data);
        js_free_rt(rt, context);
    }
}

// Main program entrypoint
int main(int argc, char *argv[])
{
    Result rc;

    print_console = consoleInit(NULL);

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
    }

    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);

    nx_context_t *nx_ctx = malloc(sizeof(nx_context_t));
    memset(nx_ctx, 0, sizeof(nx_context_t));
    JS_SetContextOpaque(ctx, nx_ctx);

    size_t runtime_buffer_size;
    char *runtime_path = "romfs:/runtime.js";
    char *runtime_buffer = (char *)read_file(runtime_path, &runtime_buffer_size);
    if (runtime_buffer == NULL)
    {
        printf("Failed to initialize JS runtime\n");
    }
    else
    {
        JSValue runtime_init_result = JS_Eval(ctx, runtime_buffer, runtime_buffer_size, runtime_path, JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(runtime_init_result))
        {
            print_js_error(ctx);
        }
        JS_FreeValue(ctx, runtime_init_result);
        free(runtime_buffer);
    }

    JS_NewClassID(&js_canvas_context_class_id);
    JSClassDef canvas_context_class = {
        "CanvasContext2D",
        .finalizer = finalizer_canvas_context_2d,
    };
    JS_NewClass(rt, js_canvas_context_class_id, &canvas_context_class);

    JSValue global_obj = JS_GetGlobalObject(ctx);
    JSValue switch_obj = JS_GetPropertyStr(ctx, global_obj, "Switch");
    JSValue native_obj = JS_GetPropertyStr(ctx, switch_obj, "native");
    JSValue switch_dispatch_func = JS_GetPropertyStr(ctx, switch_obj, "dispatchEvent");

    nx_init_applet(ctx, native_obj);
    nx_init_font(ctx, native_obj);
    nx_init_fs(ctx, native_obj);

    JSValue exit_func = JS_NewCFunction(ctx, js_exit, "exit", 0);
    JS_SetPropertyStr(ctx, switch_obj, "exit", exit_func);

    const JSCFunctionListEntry function_list[] = {
        JS_CFUNC_DEF("print", 1, js_print),
        JS_CFUNC_DEF("cwd", 0, js_cwd),

        // env vars
        JS_CFUNC_DEF("getenv", 1, js_getenv),
        JS_CFUNC_DEF("setenv", 2, js_setenv),
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

        // canvas
        JS_CFUNC_DEF("canvasNewContext", 0, js_canvas_new_context),
        JS_CFUNC_DEF("canvasSetLineWidth", 0, js_canvas_set_line_width),
        JS_CFUNC_DEF("canvasSetFillStyle", 0, js_canvas_set_fill_style),
        JS_CFUNC_DEF("canvasSetFont", 0, js_canvas_set_font),
        JS_CFUNC_DEF("canvasFillRect", 0, js_canvas_fill_rect),
        JS_CFUNC_DEF("canvasFillText", 0, js_canvas_fill_text),
        JS_CFUNC_DEF("canvasMeasureText", 0, js_canvas_measure_text),
        JS_CFUNC_DEF("canvasGetImageData", 0, js_canvas_get_image_data),
        JS_CFUNC_DEF("canvasPutImageData", 0, js_canvas_put_image_data),
    };
    JS_SetPropertyFunctionList(ctx, native_obj, function_list, sizeof(function_list) / sizeof(function_list[0]));

    // `Switch.entrypoint`
    JS_SetPropertyStr(ctx, switch_obj, "entrypoint", JS_NewString(ctx, js_path));

    // `Switch.argv`
    JSValue argv_array = JS_NewArray(ctx);
    for (int i = 0; i < argc; i++)
    {
        JS_SetPropertyUint32(ctx, argv_array, 0, JS_NewString(ctx, argv[i]));
    }
    JS_SetPropertyStr(ctx, switch_obj, "argv", argv_array);

    // Run the user code
    if (user_code != NULL)
    {
        JSValue user_code_result = JS_Eval(ctx, user_code, user_code_size, js_path, JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(user_code_result))
        {
            print_js_error(ctx);
        }
        JS_FreeValue(ctx, user_code_result);
        free(user_code);
        if (js_path_needs_free)
        {
            free(js_path);
        }
    }

    // Main loop
    while (appletMainLoop())
    {
        // Process any Promises that need to be fulfilled
        JSContext *ctx1;
        int err;
        for (;;)
        {
            err = JS_ExecutePendingJob(rt, &ctx1);
            if (err <= 0)
            {
                if (err < 0)
                {
                    print_js_error(ctx1);
                }
                break;
            }
        }

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

    // Dispatch "exit" event
    JSValue event_obj = JS_NewObject(ctx);
    JSValue event_type = JS_NewString(ctx, "exit");
    JS_SetPropertyStr(ctx, event_obj, "type", event_type);
    JSValue args[] = {event_obj};
    JSValue ret_val = JS_Call(ctx, switch_dispatch_func, switch_obj, 1, args);
    JS_FreeValue(ctx, event_obj);
    JS_FreeValue(ctx, ret_val);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    if (nx_ctx->ft_library)
    {
        FT_Done_FreeType(nx_ctx->ft_library);
    }

    free(nx_ctx);

    return 0;
}
