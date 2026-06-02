// UDP over libuv (uv_poll_t on raw fds; same model as tcp.cc).
#include "error.h"
#include "types.h"
#include "util.h"
#include "wrap.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace v8;

namespace {

void set_nonblocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags >= 0)
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Persistent datagram socket: a uv_poll_t kept readable for the socket's life.
struct dgram_t {
	uv_poll_t poll;
	Isolate *iso;
	int fd;
	Global<Function> callback;
	bool active;
	uint8_t recv_buffer[65536];
};

dgram_t *get_dgram(Local<Value> v) { return nx::Unwrap<dgram_t>(v); }

void recv_cb(uv_poll_t *handle, int status, int events) {
	dgram_t *d = static_cast<dgram_t *>(handle->data);
	Isolate *iso = d->iso;
	HandleScope scope(iso);
	Local<Context> context = iso->GetCurrentContext();
	Context::Scope cs(context);

	Local<Value> args[4] = {Undefined(iso), Undefined(iso), Undefined(iso),
	                        Undefined(iso)};
	if (status < 0) {
		args[0] = Exception::Error(nx_str(iso, uv_strerror(status)));
	} else {
		struct sockaddr_in remote;
		socklen_t rlen = sizeof(remote);
		ssize_t n = recvfrom(d->fd, d->recv_buffer, sizeof(d->recv_buffer), 0,
		                     (struct sockaddr *)&remote, &rlen);
		if (n < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				return;
			args[0] = Exception::Error(nx_str(iso, strerror(errno)));
		} else {
			Local<ArrayBuffer> ab = ArrayBuffer::New(iso, (size_t)n);
			memcpy(ab->Data(), d->recv_buffer, n);
			args[1] = ab;
			char ip[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &remote.sin_addr, ip, sizeof(ip));
			args[2] = nx_str(iso, ip);
			args[3] = Integer::New(iso, ntohs(remote.sin_port));
		}
	}
	Local<Function> cb = d->callback.Get(iso);
	TryCatch try_catch(iso);
	Local<Value> ret;
	if (!cb->Call(context, Null(iso), 4, args).ToLocal(&ret)) {
		nx_emit_error_event(iso, &try_catch);
	}
}

void nx_udp_new(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	String::Utf8Value ip(iso, info[0]);
	int port = 0;
	if (!*ip || !info[1]->Int32Value(context).To(&port)) {
		nx_throw(iso, "invalid input");
		return;
	}
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		nx_throw_errno_error(iso, errno, "socket");
		return;
	}
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	inet_pton(AF_INET, *ip, &addr.sin_addr);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		int e = errno;
		close(fd);
		nx_throw_errno_error(iso, e, "bind");
		return;
	}
	set_nonblocking(fd);
	Local<Object> obj = nx::NewWrapped(iso);
	dgram_t *d = new dgram_t();
	d->iso = iso;
	d->fd = fd;
	d->callback.Reset(iso, info[2].As<Function>());
	d->active = true;
	uv_poll_init_socket(nx_ctx(iso)->loop, &d->poll, fd);
	d->poll.data = d;
	uv_poll_start(&d->poll, UV_READABLE, recv_cb);
	nx::Wrap<dgram_t>(iso, obj, d, [](dgram_t *p) {
		if (p->active) {
			uv_poll_stop(&p->poll);
			close(p->fd);
			p->active = false;
		}
		p->callback.Reset();
		// handle close is async; leak the struct under GC (rare).
	});
	info.GetReturnValue().Set(obj);
}

// One-shot sendto: a transient uv_poll_t for writability.
struct sendto_t {
	uv_poll_t poll;
	Isolate *iso;
	int fd;
	Global<Function> callback;
	Global<Value> buffer;
	uint8_t *buf;
	size_t buf_size;
	struct sockaddr_in dest;
};

void sendto_finish(sendto_t *op, Local<Value> err, Local<Value> value) {
	Isolate *iso = op->iso;
	Local<Context> context = iso->GetCurrentContext();
	Local<Function> cb = op->callback.Get(iso);
	Local<Value> args[] = {err, value};
	uv_poll_stop(&op->poll);
	TryCatch try_catch(iso);
	Local<Value> ret;
	if (!cb->Call(context, Null(iso), 2, args).ToLocal(&ret)) {
		nx_emit_error_event(iso, &try_catch);
	}
	op->callback.Reset();
	op->buffer.Reset();
	uv_close((uv_handle_t *)&op->poll,
	         [](uv_handle_t *h) { delete static_cast<sendto_t *>(h->data); });
}

void sendto_cb(uv_poll_t *handle, int status, int events) {
	sendto_t *op = static_cast<sendto_t *>(handle->data);
	Isolate *iso = op->iso;
	HandleScope scope(iso);
	Context::Scope cs(iso->GetCurrentContext());
	if (status < 0) {
		sendto_finish(op, Exception::Error(nx_str(iso, uv_strerror(status))),
		              Undefined(iso));
		return;
	}
	ssize_t n = sendto(op->fd, op->buf, op->buf_size, 0,
	                   (struct sockaddr *)&op->dest, sizeof(op->dest));
	if (n < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return;
		sendto_finish(op, Exception::Error(nx_str(iso, strerror(errno))),
		              Undefined(iso));
		return;
	}
	sendto_finish(op, Undefined(iso), Integer::New(iso, (int)n));
}

void nx_udp_send(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	Local<Function> cb = info[0].As<Function>();
	int fd = 0, port = 0;
	size_t size = 0;
	uint8_t *buf = NX_GetBufferSource(iso, &size, info[2]);
	if (!buf || !info[1]->Int32Value(context).To(&fd) ||
	    !info[4]->Int32Value(context).To(&port)) {
		nx_throw(iso, "invalid input");
		return;
	}
	String::Utf8Value ip(iso, info[3]);
	struct sockaddr_in dest;
	memset(&dest, 0, sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons(port);
	if (!*ip || inet_pton(AF_INET, *ip, &dest.sin_addr) <= 0) {
		nx_throw(iso, "invalid IP address");
		return;
	}
	sendto_t *op = new sendto_t();
	op->iso = iso;
	op->fd = fd;
	op->callback.Reset(iso, cb);
	op->buffer.Reset(iso, info[2]);
	op->buf = buf;
	op->buf_size = size;
	op->dest = dest;
	uv_poll_init_socket(nx_ctx(iso)->loop, &op->poll, fd);
	op->poll.data = op;
	uv_poll_start(&op->poll, UV_WRITABLE, sendto_cb);
}

void nx_dgram_close(const FunctionCallbackInfo<Value> &info) {
	dgram_t *d = get_dgram(info.This());
	if (!d || !d->active)
		return;
	d->active = false;
	uv_poll_stop(&d->poll);
	d->callback.Reset();
	close(d->fd);
	d->fd = -1;
}

void nx_dgram_get_fd(const FunctionCallbackInfo<Value> &info) {
	dgram_t *d = get_dgram(info.This());
	if (!d)
		return;
	info.GetReturnValue().Set(Integer::New(info.GetIsolate(), d->fd));
}

void nx_dgram_get_address(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	dgram_t *d = get_dgram(info.This());
	if (!d)
		return;
	if (d->fd < 0) {
		nx_throw(iso, "Socket is closed");
		return;
	}
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	if (getsockname(d->fd, (struct sockaddr *)&addr, &len) < 0) {
		nx_throw_errno_error(iso, errno, "getsockname");
		return;
	}
	char ip[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
	Local<Object> result = Object::New(iso);
	result->Set(context, nx_str(iso, "address"), nx_str(iso, ip)).Check();
	result->Set(context, nx_str(iso, "port"),
	            Integer::New(iso, ntohs(addr.sin_port)))
	    .Check();
	info.GetReturnValue().Set(result);
}

void nx_dgram_set_broadcast(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	dgram_t *d = get_dgram(info.This());
	if (!d)
		return;
	int enabled = info[0]->BooleanValue(iso) ? 1 : 0;
	if (setsockopt(d->fd, SOL_SOCKET, SO_BROADCAST, &enabled,
	               sizeof(enabled)) < 0) {
		nx_throw_errno_error(iso, errno, "setsockopt(SO_BROADCAST)");
	}
}

// Parse a multicast-membership request (multicastAddr, [interfaceAddr]).
bool parse_mreq(Isolate *iso, const FunctionCallbackInfo<Value> &info,
                struct ip_mreq *mreq) {
	memset(mreq, 0, sizeof(*mreq));
	String::Utf8Value mcast(iso, info[0]);
	if (!*mcast || inet_pton(AF_INET, *mcast, &mreq->imr_multiaddr) <= 0) {
		nx_throw(iso, "invalid multicast address");
		return false;
	}
	if (info.Length() > 1 && !info[1]->IsUndefined()) {
		String::Utf8Value iface(iso, info[1]);
		if (!*iface || inet_pton(AF_INET, *iface, &mreq->imr_interface) <= 0) {
			nx_throw(iso, "invalid interface address");
			return false;
		}
	} else {
		mreq->imr_interface.s_addr = htonl(INADDR_ANY);
	}
	return true;
}

void nx_dgram_add_membership(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	dgram_t *d = get_dgram(info.This());
	if (!d)
		return;
	struct ip_mreq mreq;
	if (!parse_mreq(iso, info, &mreq))
		return;
	if (setsockopt(d->fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq,
	               sizeof(mreq)) < 0) {
		nx_throw_errno_error(iso, errno, "setsockopt(IP_ADD_MEMBERSHIP)");
	}
}

void nx_dgram_drop_membership(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	dgram_t *d = get_dgram(info.This());
	if (!d)
		return;
	struct ip_mreq mreq;
	if (!parse_mreq(iso, info, &mreq))
		return;
	if (setsockopt(d->fd, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq,
	               sizeof(mreq)) < 0) {
		nx_throw_errno_error(iso, errno, "setsockopt(IP_DROP_MEMBERSHIP)");
	}
}

void nx_udp_init(const FunctionCallbackInfo<Value> &info) {
	Isolate *iso = info.GetIsolate();
	Local<Context> context = iso->GetCurrentContext();
	Local<Object> proto = info[0]
	                          .As<Object>()
	                          ->Get(context, nx_str(iso, "prototype"))
	                          .ToLocalChecked()
	                          .As<Object>();
	NX_DEF_FUNC(proto, "close", nx_dgram_close, 0);
	NX_DEF_FUNC(proto, "setBroadcast", nx_dgram_set_broadcast, 1);
	NX_DEF_FUNC(proto, "addMembership", nx_dgram_add_membership, 1);
	NX_DEF_FUNC(proto, "dropMembership", nx_dgram_drop_membership, 1);
	NX_DEF_GET(proto, "fd", nx_dgram_get_fd);
	NX_DEF_GET(proto, "address", nx_dgram_get_address);
}

} // namespace

void nx_init_udp(Isolate *iso, Local<Object> init_obj) {
	NX_SET_FUNC(init_obj, "udpInit", nx_udp_init);
	NX_SET_FUNC(init_obj, "udpNew", nx_udp_new);
	NX_SET_FUNC(init_obj, "udpSend", nx_udp_send);
}
