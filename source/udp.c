#include "udp.h"
#include "error.h"
#include "poll.h"
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// ---- DatagramSocket class ----

static JSClassID nx_dgram_class_id;

typedef struct {
	nx_recvfrom_t recv;
	nx_js_callback_t cb;
	uint8_t recv_buffer[65536]; // Max UDP datagram size
} nx_js_dgram_t;

static nx_js_dgram_t *nx_js_dgram_get(JSContext *ctx, JSValueConst obj) {
	return JS_GetOpaque2(ctx, obj, nx_dgram_class_id);
}

static void finalizer_dgram(JSRuntime *rt, JSValue val) {
	nx_js_dgram_t *data = JS_GetOpaque(val, nx_dgram_class_id);
	if (data) {
		nx_context_t *nx_ctx = JS_GetContextOpaque(data->cb.context);
		nx_remove_watcher(&nx_ctx->poll, (nx_watcher_t *)&data->recv);
		if (!JS_IsUndefined(data->cb.callback)) {
			JS_FreeValue(data->cb.context, data->cb.callback);
		}
		close(data->recv.fd);
		js_free_rt(rt, data);
	}
}

// Called each time a datagram arrives on the socket.
// Invokes the JS callback with (err, bytesRead, remoteIp, remotePort).
void nx_on_recvfrom(nx_poll_t *p, nx_recvfrom_t *req) {
	nx_js_callback_t *req_cb = (nx_js_callback_t *)req->opaque;
	JSContext *ctx = req_cb->context;

	JSValue args[] = {JS_UNDEFINED, JS_UNDEFINED, JS_UNDEFINED, JS_UNDEFINED};

	if (req->err) {
		args[0] = JS_NewError(ctx);
		JS_SetPropertyStr(ctx, args[0], "message",
						  JS_NewString(ctx, strerror(req->err)));
	} else {
		args[1] = JS_NewInt32(ctx, req->bytes_read);

		char ip_str[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &req->remote_addr.sin_addr, ip_str,
				  sizeof(ip_str));
		args[2] = JS_NewString(ctx, ip_str);
		args[3] = JS_NewInt32(ctx, ntohs(req->remote_addr.sin_port));
	}

	JSValue ret_val = JS_Call(ctx, req_cb->callback, JS_NULL, 4, args);
	JS_FreeValue(ctx, args[0]);
	JS_FreeValue(ctx, args[1]);
	JS_FreeValue(ctx, args[2]);
	JS_FreeValue(ctx, args[3]);
	if (JS_IsException(ret_val)) {
		nx_emit_error_event(ctx);
	}
	JS_FreeValue(ctx, ret_val);
}

// $.udpNew(ip, port, onRecvCallback) -> DatagramSocket opaque object
static JSValue nx_js_udp_new(JSContext *ctx, JSValueConst this_val, int argc,
							 JSValueConst *argv) {
	const char *ip = JS_ToCString(ctx, argv[0]);
	int port;
	if (!ip || JS_ToInt32(ctx, &port, argv[1])) {
		if (ip)
			JS_FreeCString(ctx, ip);
		JS_ThrowTypeError(ctx, "invalid input");
		return JS_EXCEPTION;
	}

	JSValue obj = JS_NewObjectClass(ctx, nx_dgram_class_id);
	nx_js_dgram_t *data = js_mallocz(ctx, sizeof(nx_js_dgram_t));
	if (!data) {
		JS_FreeCString(ctx, ip);
		JS_ThrowOutOfMemory(ctx);
		return JS_EXCEPTION;
	}
	JS_SetOpaque(obj, data);

	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	data->cb.context = ctx;
	data->cb.callback = JS_DupValue(ctx, argv[2]);
	data->cb.buffer = JS_UNDEFINED;
	data->recv.opaque = &data->cb;

	int r = nx_udp_new(&nx_ctx->poll, &data->recv, ip, port,
					   data->recv_buffer, sizeof(data->recv_buffer),
					   nx_on_recvfrom);
	JS_FreeCString(ctx, ip);

	if (r < 0) {
		JS_FreeValue(ctx, obj);
		JS_ThrowTypeError(ctx, strerror(errno));
		return JS_EXCEPTION;
	}

	return obj;
}

// ---- sendto ----

void nx_on_sendto(nx_poll_t *p, nx_sendto_t *req) {
	nx_js_callback_t *req_cb = (nx_js_callback_t *)req->opaque;
	JSContext *ctx = req_cb->context;
	JS_FreeValue(ctx, req_cb->buffer);

	JSValue args[] = {JS_UNDEFINED, JS_UNDEFINED};

	if (req->err) {
		args[0] = JS_NewError(ctx);
		JS_SetPropertyStr(ctx, args[0], "message",
						  JS_NewString(ctx, strerror(req->err)));
	} else {
		args[1] = JS_NewInt32(ctx, req->bytes_written);
	}

	JSValue ret_val = JS_Call(ctx, req_cb->callback, JS_NULL, 2, args);
	JS_FreeValue(ctx, args[0]);
	JS_FreeValue(ctx, args[1]);
	JS_FreeValue(ctx, req_cb->callback);
	if (JS_IsException(ret_val)) {
		nx_emit_error_event(ctx);
	}
	JS_FreeValue(ctx, ret_val);
	js_free(ctx, req_cb);
	js_free(ctx, req);
}

// $.udpSend(callback, fd, data, ip, port)
static JSValue nx_js_udp_send(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	size_t buffer_size;
	JSValue buffer_val = JS_DupValue(ctx, argv[2]);
	uint8_t *buffer = JS_GetArrayBuffer(ctx, &buffer_size, buffer_val);
	int fd;
	int port;
	if (!buffer || JS_ToInt32(ctx, &fd, argv[1]) ||
		JS_ToInt32(ctx, &port, argv[4])) {
		JS_FreeValue(ctx, buffer_val);
		JS_ThrowTypeError(ctx, "invalid input");
		return JS_EXCEPTION;
	}

	const char *ip = JS_ToCString(ctx, argv[3]);
	if (!ip) {
		JS_FreeValue(ctx, buffer_val);
		return JS_EXCEPTION;
	}

	struct sockaddr_in dest = {
		.sin_family = AF_INET,
		.sin_port = htons(port),
	};
	if (inet_pton(AF_INET, ip, &dest.sin_addr) <= 0) {
		JS_FreeCString(ctx, ip);
		JS_FreeValue(ctx, buffer_val);
		JS_ThrowTypeError(ctx, "invalid IP address");
		return JS_EXCEPTION;
	}
	JS_FreeCString(ctx, ip);

	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	nx_sendto_t *req = js_malloc(ctx, sizeof(nx_sendto_t));
	nx_js_callback_t *req_cb = js_malloc(ctx, sizeof(nx_js_callback_t));
	req_cb->context = ctx;
	req_cb->callback = JS_DupValue(ctx, argv[0]);
	req_cb->buffer = buffer_val;
	req->opaque = req_cb;

	nx_sendto(&nx_ctx->poll, req, fd, buffer, buffer_size, &dest,
			  nx_on_sendto);

	return JS_UNDEFINED;
}

// ---- DatagramSocket prototype methods ----

// dgramSocket.close()
static JSValue nx_js_dgram_close(JSContext *ctx, JSValueConst this_val,
								 int argc, JSValueConst *argv) {
	nx_js_dgram_t *data = nx_js_dgram_get(ctx, this_val);
	if (!data)
		return JS_EXCEPTION;
	nx_context_t *nx_ctx = JS_GetContextOpaque(ctx);
	nx_remove_watcher(&nx_ctx->poll, (nx_watcher_t *)&data->recv);
	JS_FreeValue(ctx, data->cb.callback);
	data->cb.callback = JS_UNDEFINED;
	close(data->recv.fd);
	return JS_UNDEFINED;
}

// dgramSocket.fd (getter) - expose fd for sendto
static JSValue nx_js_dgram_get_fd(JSContext *ctx, JSValueConst this_val,
								  int argc, JSValueConst *argv) {
	nx_js_dgram_t *data = nx_js_dgram_get(ctx, this_val);
	if (!data)
		return JS_EXCEPTION;
	return JS_NewInt32(ctx, data->recv.fd);
}

// dgramSocket.address (getter) - local bound address
static JSValue nx_js_dgram_get_address(JSContext *ctx, JSValueConst this_val,
									   int argc, JSValueConst *argv) {
	nx_js_dgram_t *data = nx_js_dgram_get(ctx, this_val);
	if (!data)
		return JS_EXCEPTION;

	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	if (getsockname(data->recv.fd, (struct sockaddr *)&addr, &len) < 0) {
		return JS_UNDEFINED;
	}

	char ip_str[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str));

	JSValue result = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, result, "address", JS_NewString(ctx, ip_str));
	JS_SetPropertyStr(ctx, result, "port",
					  JS_NewInt32(ctx, ntohs(addr.sin_port)));
	return result;
}

// dgramSocket.setBroadcast(enabled)
static JSValue nx_js_dgram_set_broadcast(JSContext *ctx, JSValueConst this_val,
										 int argc, JSValueConst *argv) {
	nx_js_dgram_t *data = nx_js_dgram_get(ctx, this_val);
	if (!data)
		return JS_EXCEPTION;
	int enabled = JS_ToBool(ctx, argv[0]);
	if (setsockopt(data->recv.fd, SOL_SOCKET, SO_BROADCAST, &enabled,
				   sizeof(enabled)) < 0) {
		JS_ThrowTypeError(ctx, "setsockopt(SO_BROADCAST): %s",
						  strerror(errno));
		return JS_EXCEPTION;
	}
	return JS_UNDEFINED;
}

// dgramSocket.addMembership(multicastAddr, interfaceAddr?)
static JSValue nx_js_dgram_add_membership(JSContext *ctx,
										  JSValueConst this_val, int argc,
										  JSValueConst *argv) {
	nx_js_dgram_t *data = nx_js_dgram_get(ctx, this_val);
	if (!data)
		return JS_EXCEPTION;

	const char *mcast_addr = JS_ToCString(ctx, argv[0]);
	if (!mcast_addr)
		return JS_EXCEPTION;

	struct ip_mreq mreq;
	memset(&mreq, 0, sizeof(mreq));
	if (inet_pton(AF_INET, mcast_addr, &mreq.imr_multiaddr) <= 0) {
		JS_FreeCString(ctx, mcast_addr);
		JS_ThrowTypeError(ctx, "invalid multicast address");
		return JS_EXCEPTION;
	}
	JS_FreeCString(ctx, mcast_addr);

	if (argc > 1 && !JS_IsUndefined(argv[1])) {
		const char *iface_addr = JS_ToCString(ctx, argv[1]);
		if (!iface_addr)
			return JS_EXCEPTION;
		inet_pton(AF_INET, iface_addr, &mreq.imr_interface);
		JS_FreeCString(ctx, iface_addr);
	} else {
		mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	}

	if (setsockopt(data->recv.fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq,
				   sizeof(mreq)) < 0) {
		JS_ThrowTypeError(ctx, "setsockopt(IP_ADD_MEMBERSHIP): %s",
						  strerror(errno));
		return JS_EXCEPTION;
	}
	return JS_UNDEFINED;
}

// dgramSocket.dropMembership(multicastAddr, interfaceAddr?)
static JSValue nx_js_dgram_drop_membership(JSContext *ctx,
										   JSValueConst this_val, int argc,
										   JSValueConst *argv) {
	nx_js_dgram_t *data = nx_js_dgram_get(ctx, this_val);
	if (!data)
		return JS_EXCEPTION;

	const char *mcast_addr = JS_ToCString(ctx, argv[0]);
	if (!mcast_addr)
		return JS_EXCEPTION;

	struct ip_mreq mreq;
	memset(&mreq, 0, sizeof(mreq));
	if (inet_pton(AF_INET, mcast_addr, &mreq.imr_multiaddr) <= 0) {
		JS_FreeCString(ctx, mcast_addr);
		JS_ThrowTypeError(ctx, "invalid multicast address");
		return JS_EXCEPTION;
	}
	JS_FreeCString(ctx, mcast_addr);

	if (argc > 1 && !JS_IsUndefined(argv[1])) {
		const char *iface_addr = JS_ToCString(ctx, argv[1]);
		if (!iface_addr)
			return JS_EXCEPTION;
		inet_pton(AF_INET, iface_addr, &mreq.imr_interface);
		JS_FreeCString(ctx, iface_addr);
	} else {
		mreq.imr_interface.s_addr = htonl(INADDR_ANY);
	}

	if (setsockopt(data->recv.fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq,
				   sizeof(mreq)) < 0) {
		JS_ThrowTypeError(ctx, "setsockopt(IP_DROP_MEMBERSHIP): %s",
						  strerror(errno));
		return JS_EXCEPTION;
	}
	return JS_UNDEFINED;
}

// dgramSocket.recvBuffer (getter) - expose the internal receive buffer as ArrayBuffer
static JSValue nx_js_dgram_get_recv_buffer(JSContext *ctx,
										   JSValueConst this_val, int argc,
										   JSValueConst *argv) {
	nx_js_dgram_t *data = nx_js_dgram_get(ctx, this_val);
	if (!data)
		return JS_EXCEPTION;
	// Return an ArrayBuffer that references the recv_buffer (no copy).
	// The buffer is owned by the dgram struct, so we don't set a free func.
	return JS_NewArrayBuffer(ctx, data->recv_buffer,
							 sizeof(data->recv_buffer), NULL, NULL, false);
}

// Init function: installs prototype methods on the DatagramSocket class
static JSValue nx_js_udp_init(JSContext *ctx, JSValueConst this_val, int argc,
							  JSValueConst *argv) {
	JSAtom atom;
	JSValue proto = JS_GetPropertyStr(ctx, argv[0], "prototype");
	NX_DEF_FUNC(proto, "close", nx_js_dgram_close, 0);
	NX_DEF_FUNC(proto, "setBroadcast", nx_js_dgram_set_broadcast, 1);
	NX_DEF_FUNC(proto, "addMembership", nx_js_dgram_add_membership, 1);
	NX_DEF_FUNC(proto, "dropMembership", nx_js_dgram_drop_membership, 1);
	NX_DEF_GET(proto, "fd", nx_js_dgram_get_fd);
	NX_DEF_GET(proto, "address", nx_js_dgram_get_address);
	NX_DEF_GET(proto, "recvBuffer", nx_js_dgram_get_recv_buffer);
	JS_FreeValue(ctx, proto);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry function_list[] = {
	JS_CFUNC_DEF("udpInit", 1, nx_js_udp_init),
	JS_CFUNC_DEF("udpNew", 3, nx_js_udp_new),
	JS_CFUNC_DEF("udpSend", 5, nx_js_udp_send),
};

void nx_init_udp(JSContext *ctx, JSValueConst init_obj) {
	JSRuntime *rt = JS_GetRuntime(ctx);

	JS_NewClassID(rt, &nx_dgram_class_id);
	JSClassDef nx_dgram_class = {
		"DatagramSocket",
		.finalizer = finalizer_dgram,
	};
	JS_NewClass(rt, nx_dgram_class_id, &nx_dgram_class);

	JS_SetPropertyFunctionList(ctx, init_obj, function_list,
							   countof(function_list));
}
