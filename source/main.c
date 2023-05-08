// Include the most common headers from the C standard library
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>

// Include the main libnx system header, for Switch development
#include <switch.h>

#include "duktape.h"

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

static duk_ret_t native_print(duk_context *ctx)
{
    duk_push_string(ctx, " ");
    duk_insert(ctx, 0);
    duk_join(ctx, duk_get_top(ctx) - 1);
    printf("%s\n", duk_safe_to_string(ctx, -1));
    return 0;
}

static duk_ret_t native_cwd(duk_context *ctx)
{
    char cwd[1024]; // buffer to hold current working directory

    if (getcwd(cwd, sizeof(cwd)) != NULL)
    {
        duk_push_string(ctx, cwd);
    }
    else
    {
        duk_push_null(ctx);
    }
    return 1;
}

static duk_ret_t get_env_variables(duk_context *ctx)
{
    // Push a new object onto the stack to store the environment variables
    duk_push_object(ctx);

    // Get the environment variables from the operating system
    extern char **environ;
    char **envp = environ;
    while (*envp)
    {
        // Split each environment variable into a key-value pair
        char *eq = strchr(*envp, '=');
        if (eq)
        {
            *eq = '\0';
            char *key = *envp;
            char *value = eq + 1;

            // Add the key-value pair to the object on the stack
            duk_push_string(ctx, value);
            duk_put_prop_string(ctx, -2, key);
        }
        envp++;
    }

    // Return the object with the environment variables
    return 1;
}

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

    char *buffer = read_file(js_path);
    if (buffer == NULL)
    {
        printf("Failed to load JS file: %s\n", js_path);
    }

    duk_context *ctx = duk_create_heap_default();

    // The global `Switch` object
    duk_push_object(ctx);

    // `Switch.print()`
    duk_push_c_function(ctx, native_print, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "print");

    duk_push_c_function(ctx, get_env_variables, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "env");

    // `Switch.cwd()`
    duk_push_c_function(ctx, native_cwd, DUK_VARARGS);
    duk_put_prop_string(ctx, -2, "cwd");

    // `Switch.argv`
    duk_push_array(ctx);
    for (int i = 0; i < argc; i++)
    {
        duk_push_string(ctx, argv[i]);
        duk_put_prop_index(ctx, -2, i);
    }
    duk_put_prop_string(ctx, -2, "argv");

    duk_put_global_string(ctx, "Switch");

    // Initialize the JS runtime environment
    Result rc = romfsInit();
    if (R_FAILED(rc))
    {
        printf("romfsInit failed: %08X\n", rc);
    }
    else
    {
        char *runtime_buffer = read_file("romfs:/runtime.js");
        if (runtime_buffer == NULL)
        {
            printf("Failed to initialize JS runtime\n");
        }
        else
        {
            duk_eval_string(ctx, runtime_buffer);
            free(runtime_buffer);
            duk_pop(ctx); /* pop eval result */
        }
    }

    // Run the user code
    if (buffer != NULL)
    {
        duk_eval_string(ctx, buffer);
        free(buffer);
        duk_pop(ctx); /* pop eval result */
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
            // TODO: Fire keypress event
            printf("kDown: %" PRIu64 "\n", kDown);
        }

        // Your code goes here

        // Update the console, sending a new frame to the display
        consoleUpdate(NULL);
    }

    duk_destroy_heap(ctx);

    // Deinitialize and clean up resources used by the console (important!)
    consoleExit(NULL);
    return 0;
}
