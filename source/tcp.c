#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "tcp.h"
#include "poll.h"
#include "error.h"

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
    int port;
    const char *ip = JS_ToCString(ctx, argv[1]);
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
    req_cb->buffer = JS_UNDEFINED;
    req->opaque = req_cb;

    nx_tcp_connect(&nx_ctx->poll, req, ip, port, nx_on_connect);
    JS_FreeCString(ctx, ip);

    return JS_UNDEFINED;
}

void nx_on_read(nx_poll_t *p, nx_read_t *req)
{
    nx_js_callback_t *req_cb = (nx_js_callback_t *)req->opaque;
    JSContext *ctx = req_cb->context;
    JS_FreeValue(ctx, req_cb->buffer);

    JSValue args[] = {JS_UNDEFINED, JS_UNDEFINED};

    if (req->err)
    {
        /* Error during read. */
        args[0] = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, args[0], "message", JS_NewString(ctx, strerror(req->err)));
    }
    else
    {
        args[1] = JS_NewInt32(ctx, req->bytes_read);
    }

    JSValue ret_val = JS_Call(ctx, req_cb->callback, JS_NULL, 2, args);
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
    JS_FreeValue(ctx, req_cb->callback);
    if (JS_IsException(ret_val))
    {
        nx_emit_error_event(ctx);
    }
    JS_FreeValue(ctx, ret_val);
    free(req_cb);
    free(req);
}

JSValue nx_js_tcp_read(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    size_t buffer_size;
    JSValue buffer_value = JS_DupValue(ctx, argv[2]);
    uint8_t *buffer = JS_GetArrayBuffer(ctx, &buffer_size, buffer_value);
    int fd;
    if (!buffer || JS_ToInt32(ctx, &fd, argv[1]))
    {
        return JS_EXCEPTION;
    }

    nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
    nx_read_t *req = malloc(sizeof(nx_read_t));
    nx_js_callback_t *req_cb = malloc(sizeof(nx_js_callback_t));
    req_cb->context = ctx;
    req_cb->callback = JS_DupValue(ctx, argv[0]);
    req_cb->buffer = buffer_value;
    req->opaque = req_cb;

    nx_read(&nx_ctx->poll, req, fd, buffer, buffer_size, nx_on_read);

    return JS_UNDEFINED;
}

void nx_on_write(nx_poll_t *p, nx_write_t *req)
{
    nx_js_callback_t *req_cb = (nx_js_callback_t *)req->opaque;
    JSContext *ctx = req_cb->context;
    JS_FreeValue(ctx, req_cb->buffer);

    JSValue args[] = {JS_UNDEFINED, JS_UNDEFINED};

    if (req->err)
    {
        /* Error during write. */
        args[0] = JS_NewError(ctx);
        JS_SetPropertyStr(ctx, args[0], "message", JS_NewString(ctx, strerror(req->err)));
    }
    else
    {
        args[1] = JS_NewInt32(ctx, req->bytes_written);
    }

    JSValue ret_val = JS_Call(ctx, req_cb->callback, JS_NULL, 2, args);
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);
    JS_FreeValue(ctx, req_cb->callback);
    if (JS_IsException(ret_val))
    {
        nx_emit_error_event(ctx);
    }
    JS_FreeValue(ctx, ret_val);
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
    req_cb->buffer = buffer_val;
    req->opaque = req_cb;

    nx_write(&nx_ctx->poll, req, fd, buffer, buffer_size, nx_on_write);

    return JS_UNDEFINED;
}

JSValue nx_js_tcp_close(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    int fd;
    if (JS_ToInt32(ctx, &fd, argv[0])) {
        return JS_EXCEPTION;
    }
    if (close(fd)) {
        // TODO: Throw a regular error
        JS_ThrowTypeError(ctx, strerror(errno));
        return JS_EXCEPTION;
    }
    return JS_UNDEFINED;
}

static JSClassID nx_tcp_server_class_id;

typedef struct
{
    nx_server_t server;
    nx_js_callback_t cb;
} nx_js_tcp_server_t;

static nx_js_tcp_server_t *nx_js_tcp_server_get(JSContext *ctx, JSValueConst obj)
{
    return JS_GetOpaque2(ctx, obj, nx_tcp_server_class_id);
}

static void finalizer_tcp_server(JSRuntime *rt, JSValue val)
{
    fprintf(stderr, "finalizer_tcp_server\n");
    nx_js_tcp_server_t *data = JS_GetOpaque(val, nx_tcp_server_class_id);
    if (data)
    {
        nx_context_t *nx_ctx = JS_GetContextOpaque(data->cb.context);
        nx_remove_watcher(&nx_ctx->poll, (nx_watcher_t*)&data->server);
        if (!JS_IsUndefined(data->cb.callback)) {
            JS_FreeValue(data->cb.context, data->cb.callback);
        }
        js_free_rt(rt, data);
    }
}

void nx_on_accept(nx_poll_t *p, nx_server_t *req, int client_fd) {
    // Handle new client connection here
    // client_fd is the file descriptor for the new client
    nx_js_callback_t *req_cb = (nx_js_callback_t *)req->opaque;
    JSValue args[] = {JS_NewInt32(req_cb->context, client_fd)};
    JSValue ret_val = JS_Call(req_cb->context, req_cb->callback, JS_NULL, 1, args);
    if (JS_IsException(ret_val))
    {
        nx_emit_error_event(req_cb->context);
    }
    JS_FreeValue(req_cb->context, ret_val);
}

JSValue nx_js_tcp_server_new(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    const char *ip = JS_ToCString(ctx, argv[0]);
    int port;
    if (!ip || JS_ToInt32(ctx, &port, argv[1]))
    {
        JS_ThrowTypeError(ctx, "invalid input");
        return JS_EXCEPTION;
    }

    JSValue obj = JS_NewObjectClass(ctx, nx_tcp_server_class_id);
    nx_js_tcp_server_t *data = js_mallocz(ctx, sizeof(nx_js_tcp_server_t));
    if (!data)
    {
        JS_ThrowOutOfMemory(ctx);
        return JS_EXCEPTION;
    }
    JS_SetOpaque(obj, data);

    nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
    data->cb.context = ctx;
    data->cb.callback = JS_DupValue(ctx, argv[2]);
    data->cb.buffer = JS_UNDEFINED;
    data->server.opaque = &data->cb;

    int fd = nx_tcp_server(&nx_ctx->poll, &data->server, ip, port, nx_on_accept);
    JS_FreeCString(ctx, ip);

    if (fd < 0) {
        JS_ThrowTypeError(ctx, strerror(errno));
        return JS_EXCEPTION;
    }

    return obj;
}

JSValue nx_js_tcp_server_close(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
    nx_js_tcp_server_t *data = nx_js_tcp_server_get(ctx, this_val);
    nx_remove_watcher(&nx_ctx->poll, (nx_watcher_t*)&data->server);
    JS_FreeValue(ctx, data->cb.callback);
    data->cb.callback = JS_UNDEFINED;
    shutdown(data->server.fd, SHUT_RDWR);
    close(data->server.fd);
    return JS_UNDEFINED;
}

static JSValue nx_js_tcp_init_server(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
    NX_DEF_FUNC(proto, "close", nx_js_tcp_server_close, 0);
    JS_FreeValue(ctx, proto);
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
    JS_CFUNC_DEF("connect", 1, nx_js_tcp_connect),
    JS_CFUNC_DEF("read", 1, nx_js_tcp_read),
    JS_CFUNC_DEF("write", 1, nx_js_tcp_write),
    JS_CFUNC_DEF("close", 1, nx_js_tcp_close),

    JS_CFUNC_DEF("tcpServerInit", 1, nx_js_tcp_init_server),
    JS_CFUNC_DEF("tcpServerNew", 3, nx_js_tcp_server_new)};

void nx_init_tcp(JSContext *ctx, JSValueConst init_obj)
{
    JSRuntime *rt = JS_GetRuntime(ctx);

    /* TCP Server */
    JS_NewClassID(&nx_tcp_server_class_id);
    JSClassDef nx_tcp_server_class = {
        "Server",
        .finalizer = finalizer_tcp_server,
    };
    JS_NewClass(rt, nx_tcp_server_class_id, &nx_tcp_server_class);

    JS_SetPropertyFunctionList(ctx, init_obj, function_list, countof(function_list));
}
