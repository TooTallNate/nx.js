#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <inttypes.h>
#include <switch.h>
#include <quickjs.h>

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
    /* An exception was thrown */
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

//`static duk_ret_t get_env_variables(duk_context *ctx)
//`{
//`    // Push a new object onto the stack to store the environment variables
//`    duk_push_object(ctx);
//`
//`    // Get the environment variables from the operating system
//`    extern char **environ;
//`    char **envp = environ;
//`    while (*envp)
//`    {
//`        // Split each environment variable into a key-value pair
//`        char *eq = strchr(*envp, '=');
//`        if (eq)
//`        {
//`            *eq = '\0';
//`            char *key = *envp;
//`            char *value = eq + 1;
//`
//`            // Add the key-value pair to the object on the stack
//`            duk_push_string(ctx, value);
//`            duk_put_prop_string(ctx, -2, key);
//`        }
//`        envp++;
//`    }
//`
//`    // Return the object with the environment variables
//`    return 1;
//`}

// Main program entrypoint
int main(int argc, char *argv[])
{
    // This example uses a text console, as a simple way to output text to the screen.
    // If you want to write a software-rendered graphics application,
    //   take a look at the graphics/simplegfx example, which uses the libnx Framebuffer API instead.
    // If on the other hand you want to write an OpenGL based application,
    //   take a look at the graphics/opengl set of examples, which uses EGL instead.
    consoleInit(NULL);

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

    printf("JS Path: %s\n", js_path);
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
    JSValue switch_dispatch_func = JS_GetPropertyStr(ctx, switch_obj, "dispatchEvent");

    JSValue print_func = JS_NewCFunction(ctx, js_print, "print", 0);
    JS_SetPropertyStr(ctx, switch_obj, "print", print_func);

    JSValue cwd_func = JS_NewCFunction(ctx, js_cwd, "cwd", 0);
    JS_SetPropertyStr(ctx, switch_obj, "cwd", cwd_func);

    ///* place the object on the global scope */
    // JS_SetPropertyStr(ctx, global_obj, "Switch", switch_obj);

    // duk_push_c_function(ctx, get_env_variables, DUK_VARARGS);
    // duk_put_prop_string(ctx, -2, "env");

    //// `Switch.argv`
    // duk_push_array(ctx);
    // for (int i = 0; i < argc; i++)
    //{
    //     /duk_push_string(ctx, argv[i]);
    //     /duk_put_prop_index(ctx, -2, i);
    // }
    // duk_put_prop_string(ctx, -2, "argv");

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
            break; // break in order to return to hbmenu

        if (kDown)
        {
            JSValue event_obj = JS_NewObject(ctx);
            JSValue event_type = JS_NewString(ctx, "input");
            JS_SetPropertyStr(ctx, event_obj, "type", event_type);
            // TODO: "bigint is not supported"
            // JSValue event_raw = JS_NewBigUint64(ctx, kDown);
            JSValue event_raw = JS_NewUint32(ctx, kDown);
            JS_SetPropertyStr(ctx, event_obj, "value", event_raw);
            JSValue args[] = {event_obj};
            JSValue ret_val = JS_Call(ctx, switch_dispatch_func, switch_obj, 1, args);
            if (JS_IsException(ret_val))
            {
                print_js_error(ctx);
            }
            JS_FreeValue(ctx, event_raw);
            JS_FreeValue(ctx, event_type);
            JS_FreeValue(ctx, event_obj);
            JS_FreeValue(ctx, ret_val);
        }

        // Update the console, sending a new frame to the display
        consoleUpdate(NULL);
    }

    JS_FreeValue(ctx, switch_dispatch_func);
    JS_FreeValue(ctx, switch_obj);
    JS_FreeValue(ctx, global_obj);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);

    // Deinitialize and clean up resources used by the console (important!)
    consoleExit(NULL);
    return 0;
}
