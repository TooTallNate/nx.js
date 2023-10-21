#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "tcp.h"
#include "poll.h"
#include "error.h"

typedef struct
{
    JSContext *context;
    JSValue callback;
} nx_js_callback_t;

void nx_on_connect(nx_poll_t *p, nx_connect_t *req)
{
    nx_js_callback_t *req_cb = (nx_js_callback_t *)req->opaque;
    JSValue args[] = {JS_UNDEFINED, JS_UNDEFINED};

    if (req->err)
    {
        /* Error during connect. */
        // printf("Error occurred during connect: %s\n", strerror(so_error));
    }
    else
    {
        // printf("Connection established.\n");
        args[1] = JS_NewInt32(req_cb->context, req->fd);
    }

    JSValue ret_val = JS_Call(req_cb->context, req_cb->callback, JS_NULL, 2, args);
    JS_FreeValue(req_cb->context, req_cb->callback);
    if (JS_IsException(ret_val))
    {
        nx_emit_error_event(req_cb->context);
    }
    JS_FreeValue(req_cb->context, ret_val);
    free(req_cb);
    free(req);
}

JSValue nx_js_tcp_connect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *ip = JS_ToCString(ctx, argv[1]);
    int port;
    if (!ip || JS_ToInt32(ctx, &port, argv[2]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }

    nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
    nx_connect_t *req = malloc(sizeof(nx_connect_t));
    nx_js_callback_t *req_cb = malloc(sizeof(nx_js_callback_t));
    req_cb->context = ctx;
    req_cb->callback = JS_DupValue(ctx, argv[0]);
    req->opaque = req_cb;

    nx_tcp_connect(&nx_ctx->poll, req, ip, port, nx_on_connect);

    return JS_UNDEFINED;
}

void nx_on_read(nx_poll_t *p, nx_read_t *req)
{
    nx_js_callback_t *req_cb = (nx_js_callback_t *)req->opaque;
    JSValue args[] = {JS_UNDEFINED, JS_UNDEFINED};

    if (req->err)
    {
        /* Error during read. */
        // printf("Error occurred during connect: %s\n", strerror(so_error));
    }
    else
    {
        // printf("Connection established.\n");
        args[1] = JS_NewInt32(req_cb->context, req->bytes_read);
    }

    JSValue ret_val = JS_Call(req_cb->context, req_cb->callback, JS_NULL, 2, args);
    JS_FreeValue(req_cb->context, req_cb->callback);
    if (JS_IsException(ret_val))
    {
        nx_emit_error_event(req_cb->context);
    }
    JS_FreeValue(req_cb->context, ret_val);
    free(req_cb);
    free(req);
}

JSValue nx_js_tcp_read(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    size_t buffer_size;
    uint8_t *buffer = JS_GetArrayBuffer(ctx, &buffer_size, argv[2]);
    int fd;
    if (!buffer || JS_ToInt32(ctx, &fd, argv[1]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }

    nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
    nx_read_t *req = malloc(sizeof(nx_read_t));
    nx_js_callback_t *req_cb = malloc(sizeof(nx_js_callback_t));
    req_cb->context = ctx;
    req_cb->callback = JS_DupValue(ctx, argv[0]);
    req->opaque = req_cb;

    nx_read(&nx_ctx->poll, req, fd, buffer, buffer_size, nx_on_read);

    return JS_UNDEFINED;
}

void nx_on_write(nx_poll_t *p, nx_write_t *req)
{
    nx_js_callback_t *req_cb = (nx_js_callback_t *)req->opaque;
    JSValue args[] = {JS_UNDEFINED, JS_UNDEFINED};

    if (req->err)
    {
        /* Error during read. */
        // printf("Error occurred during connect: %s\n", strerror(so_error));
    }
    else
    {
        // printf("Connection established.\n");
        args[1] = JS_NewInt32(req_cb->context, req->bytes_written);
    }

    JSValue ret_val = JS_Call(req_cb->context, req_cb->callback, JS_NULL, 2, args);
    JS_FreeValue(req_cb->context, req_cb->callback);
    if (JS_IsException(ret_val))
    {
        nx_emit_error_event(req_cb->context);
    }
    JS_FreeValue(req_cb->context, ret_val);
    free(req_cb);
    free(req);
}

JSValue nx_js_tcp_write(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    size_t buffer_size;
    JSValue buffer_val = JS_DupValue(ctx, argv[2]);
    uint8_t *buffer = JS_GetArrayBuffer(ctx, &buffer_size, buffer_val);
    int fd;
    if (!buffer || JS_ToInt32(ctx, &fd, argv[1]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }

    nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
    nx_write_t *req = malloc(sizeof(nx_write_t));
    nx_js_callback_t *req_cb = malloc(sizeof(nx_js_callback_t));
    req_cb->context = ctx;
    req_cb->callback = JS_DupValue(ctx, argv[0]);
    req->opaque = req_cb;

    nx_write(&nx_ctx->poll, req, fd, buffer, buffer_size, nx_on_write);

    return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
    JS_CFUNC_DEF("connect", 1, nx_js_tcp_connect),
    JS_CFUNC_DEF("read", 1, nx_js_tcp_read),
    JS_CFUNC_DEF("write", 1, nx_js_tcp_write)};

void nx_init_tcp(JSContext *ctx, JSValueConst native_obj)
{
    JS_SetPropertyFunctionList(ctx, native_obj, function_list, countof(function_list));
}
