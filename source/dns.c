#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <quickjs/quickjs.h>
#include "dns.h"
#include "async.h"

void js_dns_resolve_do(nx_work_t *req)
{
    nx_dns_resolve_t *data = (nx_dns_resolve_t *)req->data;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo *result;
    int status = getaddrinfo(data->hostname, NULL, &hints, &result);
    if (status != 0)
    {
        data->err = status;
        // printf("Failed to resolve DNS: %s\n", gai_strerror(status));
        return;
    }

    struct addrinfo *rp;

    size_t count = 0;
    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        count++;
    }
    data->num_entries = count;

    if (!count)
    {
        return;
    }

    data->entries = malloc(count * sizeof(char *));

    char ip[INET6_ADDRSTRLEN];
    int i = 0;
    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        void *addr;
        if (rp->ai_family == AF_INET)
        { // IPv4 address
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)rp->ai_addr;
            addr = &(ipv4->sin_addr);
        }
        else
        { // IPv6 address
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)rp->ai_addr;
            addr = &(ipv6->sin6_addr);
        }

        inet_ntop(rp->ai_family, addr, ip, sizeof(ip));
        data->entries[i++] = strdup(ip);
    }

    freeaddrinfo(result);
}

void js_dns_resolve_cb(JSContext *ctx, nx_work_t *req, JSValue *args)
{
    nx_dns_resolve_t *data = (nx_dns_resolve_t *)req->data;
    JS_FreeCString(ctx, data->hostname);

    if (data->err)
    {
        args[0] = JS_NewError(ctx);
        JS_DefinePropertyValueStr(ctx, args[0], "message", JS_NewString(ctx, gai_strerror(data->err)), JS_PROP_WRITABLE | JS_PROP_CONFIGURABLE);
        return;
    }

    JSValue result = JS_NewArray(ctx);
    for (size_t i = 0; i < data->num_entries; i++)
    {
        JS_SetPropertyUint32(ctx, result, i, JS_NewString(ctx, data->entries[i]));
        free(data->entries[i]);
    }
    free(data->entries);
    args[1] = result;
}

JSValue js_dns_resolve(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv)
{
    NX_INIT_WORK_T(nx_dns_resolve_t);
    data->hostname = JS_ToCString(ctx, argv[1]);
    nx_queue_async(ctx, req, js_dns_resolve_do, js_dns_resolve_cb, argv[0]);
    return JS_UNDEFINED;
}
